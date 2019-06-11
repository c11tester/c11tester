#include <stdio.h>
#include <algorithm>
#include <new>
#include <stdarg.h>

#include "model.h"
#include "execution.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "common.h"
#include "clockvector.h"
#include "cyclegraph.h"
#include "datarace.h"
#include "threads-model.h"
#include "bugmessage.h"
#include "fuzzer.h"

#define INITIAL_THREAD_ID       0

/**
 * Structure for holding small ModelChecker members that should be snapshotted
 */
struct model_snapshot_members {
	model_snapshot_members() :
		/* First thread created will have id INITIAL_THREAD_ID */
		next_thread_id(INITIAL_THREAD_ID),
		used_sequence_numbers(0),
		bugs(),
		bad_synchronization(false),
		bad_sc_read(false),
		asserted(false)
	{ }

	~model_snapshot_members() {
		for (unsigned int i = 0;i < bugs.size();i++)
			delete bugs[i];
		bugs.clear();
	}

	unsigned int next_thread_id;
	modelclock_t used_sequence_numbers;
	SnapVector<bug_message *> bugs;
	/** @brief Incorrectly-ordered synchronization was made */
	bool bad_synchronization;
	bool bad_sc_read;
	bool asserted;

	SNAPSHOTALLOC
};

/** @brief Constructor */
ModelExecution::ModelExecution(ModelChecker *m,
															 Scheduler *scheduler,
															 NodeStack *node_stack) :
	model(m),
	params(NULL),
	scheduler(scheduler),
	action_trace(),
	thread_map(2),			/* We'll always need at least 2 threads */
	pthread_map(0),
	pthread_counter(0),
	obj_map(),
	condvar_waiters_map(),
	obj_thrd_map(),
	mutex_map(),
	thrd_last_action(1),
	thrd_last_fence_release(),
	node_stack(node_stack),
	priv(new struct model_snapshot_members ()),
			 mo_graph(new CycleGraph()),
	fuzzer(new Fuzzer())
{
	/* Initialize a model-checker thread, for special ModelActions */
	model_thread = new Thread(get_next_id());
	add_thread(model_thread);
	scheduler->register_engine(this);
	node_stack->register_engine(this);
}

/** @brief Destructor */
ModelExecution::~ModelExecution()
{
	for (unsigned int i = 0;i < get_num_threads();i++)
		delete get_thread(int_to_id(i));

	delete mo_graph;
	delete priv;
}

int ModelExecution::get_execution_number() const
{
	return model->get_execution_number();
}

static action_list_t * get_safe_ptr_action(HashTable<const void *, action_list_t *, uintptr_t, 4> * hash, void * ptr)
{
	action_list_t *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new action_list_t();
		hash->put(ptr, tmp);
	}
	return tmp;
}

static SnapVector<action_list_t> * get_safe_ptr_vect_action(HashTable<void *, SnapVector<action_list_t> *, uintptr_t, 4> * hash, void * ptr)
{
	SnapVector<action_list_t> *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new SnapVector<action_list_t>();
		hash->put(ptr, tmp);
	}
	return tmp;
}

action_list_t * ModelExecution::get_actions_on_obj(void * obj, thread_id_t tid) const
{
	SnapVector<action_list_t> *wrv = obj_thrd_map.get(obj);
	if (wrv==NULL)
		return NULL;
	unsigned int thread=id_to_int(tid);
	if (thread < wrv->size())
		return &(*wrv)[thread];
	else
		return NULL;
}

/** @return a thread ID for a new Thread */
thread_id_t ModelExecution::get_next_id()
{
	return priv->next_thread_id++;
}

/** @return the number of user threads created during this execution */
unsigned int ModelExecution::get_num_threads() const
{
	return priv->next_thread_id;
}

/** @return a sequence number for a new ModelAction */
modelclock_t ModelExecution::get_next_seq_num()
{
	return ++priv->used_sequence_numbers;
}

/**
 * @brief Should the current action wake up a given thread?
 *
 * @param curr The current action
 * @param thread The thread that we might wake up
 * @return True, if we should wake up the sleeping thread; false otherwise
 */
bool ModelExecution::should_wake_up(const ModelAction *curr, const Thread *thread) const
{
	const ModelAction *asleep = thread->get_pending();
	/* Don't allow partial RMW to wake anyone up */
	if (curr->is_rmwr())
		return false;
	/* Synchronizing actions may have been backtracked */
	if (asleep->could_synchronize_with(curr))
		return true;
	/* All acquire/release fences and fence-acquire/store-release */
	if (asleep->is_fence() && asleep->is_acquire() && curr->is_release())
		return true;
	/* Fence-release + store can awake load-acquire on the same location */
	if (asleep->is_read() && asleep->is_acquire() && curr->same_var(asleep) && curr->is_write()) {
		ModelAction *fence_release = get_last_fence_release(curr->get_tid());
		if (fence_release && *(get_last_action(thread->get_id())) < *fence_release)
			return true;
	}
	return false;
}

void ModelExecution::wake_up_sleeping_actions(ModelAction *curr)
{
	for (unsigned int i = 0;i < get_num_threads();i++) {
		Thread *thr = get_thread(int_to_id(i));
		if (scheduler->is_sleep_set(thr)) {
			if (should_wake_up(curr, thr))
				/* Remove this thread from sleep set */
				scheduler->remove_sleep(thr);
		}
	}
}

/** @brief Alert the model-checker that an incorrectly-ordered
 * synchronization was made */
void ModelExecution::set_bad_synchronization()
{
	priv->bad_synchronization = true;
}

/** @brief Alert the model-checker that an incorrectly-ordered
 * synchronization was made */
void ModelExecution::set_bad_sc_read()
{
	priv->bad_sc_read = true;
}

bool ModelExecution::assert_bug(const char *msg)
{
	priv->bugs.push_back(new bug_message(msg));

	if (isfeasibleprefix()) {
		set_assert();
		return true;
	}
	return false;
}

/** @return True, if any bugs have been reported for this execution */
bool ModelExecution::have_bug_reports() const
{
	return priv->bugs.size() != 0;
}

SnapVector<bug_message *> * ModelExecution::get_bugs() const
{
	return &priv->bugs;
}

/**
 * Check whether the current trace has triggered an assertion which should halt
 * its execution.
 *
 * @return True, if the execution should be aborted; false otherwise
 */
bool ModelExecution::has_asserted() const
{
	return priv->asserted;
}

/**
 * Trigger a trace assertion which should cause this execution to be halted.
 * This can be due to a detected bug or due to an infeasibility that should
 * halt ASAP.
 */
void ModelExecution::set_assert()
{
	priv->asserted = true;
}

/**
 * Check if we are in a deadlock. Should only be called at the end of an
 * execution, although it should not give false positives in the middle of an
 * execution (there should be some ENABLED thread).
 *
 * @return True if program is in a deadlock; false otherwise
 */
bool ModelExecution::is_deadlocked() const
{
	bool blocking_threads = false;
	for (unsigned int i = 0;i < get_num_threads();i++) {
		thread_id_t tid = int_to_id(i);
		if (is_enabled(tid))
			return false;
		Thread *t = get_thread(tid);
		if (!t->is_model_thread() && t->get_pending())
			blocking_threads = true;
	}
	return blocking_threads;
}

/**
 * Check if this is a complete execution. That is, have all thread completed
 * execution (rather than exiting because sleep sets have forced a redundant
 * execution).
 *
 * @return True if the execution is complete.
 */
bool ModelExecution::is_complete_execution() const
{
	for (unsigned int i = 0;i < get_num_threads();i++)
		if (is_enabled(int_to_id(i)))
			return false;
	return true;
}


/**
 * Processes a read model action.
 * @param curr is the read model action to process.
 * @param rf_set is the set of model actions we can possibly read from
 * @return True if processing this read updates the mo_graph.
 */
bool ModelExecution::process_read(ModelAction *curr, ModelVector<ModelAction *> * rf_set)
{
	while(true) {

		int index = fuzzer->selectWrite(curr, rf_set);
		const ModelAction *rf = (*rf_set)[index];


		ASSERT(rf);

		mo_graph->startChanges();
		bool updated = r_modification_order(curr, rf);
		if (!is_infeasible()) {
			read_from(curr, rf);
			mo_graph->commitChanges();
			get_thread(curr)->set_return_value(curr->get_return_value());
			return updated;
		}
		mo_graph->rollbackChanges();
		(*rf_set)[index] = rf_set->back();
		rf_set->pop_back();
	}
}

/**
 * Processes a lock, trylock, or unlock model action.  @param curr is
 * the read model action to process.
 *
 * The try lock operation checks whether the lock is taken.  If not,
 * it falls to the normal lock operation case.  If so, it returns
 * fail.
 *
 * The lock operation has already been checked that it is enabled, so
 * it just grabs the lock and synchronizes with the previous unlock.
 *
 * The unlock operation has to re-enable all of the threads that are
 * waiting on the lock.
 *
 * @return True if synchronization was updated; false otherwise
 */
bool ModelExecution::process_mutex(ModelAction *curr)
{
	cdsc::mutex *mutex = curr->get_mutex();
	struct cdsc::mutex_state *state = NULL;

	if (mutex)
		state = mutex->get_state();

	switch (curr->get_type()) {
	case ATOMIC_TRYLOCK: {
		bool success = !state->locked;
		curr->set_try_lock(success);
		if (!success) {
			get_thread(curr)->set_return_value(0);
			break;
		}
		get_thread(curr)->set_return_value(1);
	}
	//otherwise fall into the lock case
	case ATOMIC_LOCK: {
		if (curr->get_cv()->getClock(state->alloc_tid) <= state->alloc_clock)
			assert_bug("Lock access before initialization");
		state->locked = get_thread(curr);
		ModelAction *unlock = get_last_unlock(curr);
		//synchronize with the previous unlock statement
		if (unlock != NULL) {
			synchronize(unlock, curr);
			return true;
		}
		break;
	}
	case ATOMIC_WAIT:
	case ATOMIC_UNLOCK: {
		/* wake up the other threads */
		for (unsigned int i = 0;i < get_num_threads();i++) {
			Thread *t = get_thread(int_to_id(i));
			Thread *curr_thrd = get_thread(curr);
			if (t->waiting_on() == curr_thrd && t->get_pending()->is_lock())
				scheduler->wake(t);
		}

		/* unlock the lock - after checking who was waiting on it */
		state->locked = NULL;

		if (!curr->is_wait())
			break;									/* The rest is only for ATOMIC_WAIT */

		break;
	}
	case ATOMIC_NOTIFY_ALL: {
		action_list_t *waiters = get_safe_ptr_action(&condvar_waiters_map, curr->get_location());
		//activate all the waiting threads
		for (action_list_t::iterator rit = waiters->begin();rit != waiters->end();rit++) {
			scheduler->wake(get_thread(*rit));
		}
		waiters->clear();
		break;
	}
	case ATOMIC_NOTIFY_ONE: {
		action_list_t *waiters = get_safe_ptr_action(&condvar_waiters_map, curr->get_location());
		Thread * thread = fuzzer->selectNotify(waiters);
		scheduler->wake(thread);
		break;
	}

	default:
		ASSERT(0);
	}
	return false;
}

/**
 * Process a write ModelAction
 * @param curr The ModelAction to process
 * @return True if the mo_graph was updated or promises were resolved
 */
bool ModelExecution::process_write(ModelAction *curr)
{

	bool updated_mod_order = w_modification_order(curr);

	mo_graph->commitChanges();

	get_thread(curr)->set_return_value(VALUE_NONE);
	return updated_mod_order;
}

/**
 * Process a fence ModelAction
 * @param curr The ModelAction to process
 * @return True if synchronization was updated
 */
bool ModelExecution::process_fence(ModelAction *curr)
{
	/*
	 * fence-relaxed: no-op
	 * fence-release: only log the occurence (not in this function), for
	 *   use in later synchronization
	 * fence-acquire (this function): search for hypothetical release
	 *   sequences
	 * fence-seq-cst: MO constraints formed in {r,w}_modification_order
	 */
	bool updated = false;
	if (curr->is_acquire()) {
		action_list_t *list = &action_trace;
		action_list_t::reverse_iterator rit;
		/* Find X : is_read(X) && X --sb-> curr */
		for (rit = list->rbegin();rit != list->rend();rit++) {
			ModelAction *act = *rit;
			if (act == curr)
				continue;
			if (act->get_tid() != curr->get_tid())
				continue;
			/* Stop at the beginning of the thread */
			if (act->is_thread_start())
				break;
			/* Stop once we reach a prior fence-acquire */
			if (act->is_fence() && act->is_acquire())
				break;
			if (!act->is_read())
				continue;
			/* read-acquire will find its own release sequences */
			if (act->is_acquire())
				continue;

			/* Establish hypothetical release sequences */
			rel_heads_list_t release_heads;
			get_release_seq_heads(curr, act, &release_heads);
			for (unsigned int i = 0;i < release_heads.size();i++)
				synchronize(release_heads[i], curr);
			if (release_heads.size() != 0)
				updated = true;
		}
	}
	return updated;
}

/**
 * @brief Process the current action for thread-related activity
 *
 * Performs current-action processing for a THREAD_* ModelAction. Proccesses
 * may include setting Thread status, completing THREAD_FINISH/THREAD_JOIN
 * synchronization, etc.  This function is a no-op for non-THREAD actions
 * (e.g., ATOMIC_{READ,WRITE,RMW,LOCK}, etc.)
 *
 * @param curr The current action
 * @return True if synchronization was updated or a thread completed
 */
bool ModelExecution::process_thread_action(ModelAction *curr)
{
	bool updated = false;

	switch (curr->get_type()) {
	case THREAD_CREATE: {
		thrd_t *thrd = (thrd_t *)curr->get_location();
		struct thread_params *params = (struct thread_params *)curr->get_value();
		Thread *th = new Thread(get_next_id(), thrd, params->func, params->arg, get_thread(curr));
		curr->set_thread_operand(th);
		add_thread(th);
		th->set_creation(curr);
		break;
	}
	case PTHREAD_CREATE: {
		(*(uint32_t *)curr->get_location()) = pthread_counter++;

		struct pthread_params *params = (struct pthread_params *)curr->get_value();
		Thread *th = new Thread(get_next_id(), NULL, params->func, params->arg, get_thread(curr));
		curr->set_thread_operand(th);
		add_thread(th);
		th->set_creation(curr);

		if ( pthread_map.size() < pthread_counter )
			pthread_map.resize( pthread_counter );
		pthread_map[ pthread_counter-1 ] = th;

		break;
	}
	case THREAD_JOIN: {
		Thread *blocking = curr->get_thread_operand();
		ModelAction *act = get_last_action(blocking->get_id());
		synchronize(act, curr);
		updated = true;							/* trigger rel-seq checks */
		break;
	}
	case PTHREAD_JOIN: {
		Thread *blocking = curr->get_thread_operand();
		ModelAction *act = get_last_action(blocking->get_id());
		synchronize(act, curr);
		updated = true;							/* trigger rel-seq checks */
		break;						// WL: to be add (modified)
	}

	case THREAD_FINISH: {
		Thread *th = get_thread(curr);
		/* Wake up any joining threads */
		for (unsigned int i = 0;i < get_num_threads();i++) {
			Thread *waiting = get_thread(int_to_id(i));
			if (waiting->waiting_on() == th &&
					waiting->get_pending()->is_thread_join())
				scheduler->wake(waiting);
		}
		th->complete();
		updated = true;							/* trigger rel-seq checks */
		break;
	}
	case THREAD_START: {
		break;
	}
	default:
		break;
	}

	return updated;
}

/**
 * Initialize the current action by performing one or more of the following
 * actions, as appropriate: merging RMWR and RMWC/RMW actions, stepping forward
 * in the NodeStack, manipulating backtracking sets, allocating and
 * initializing clock vectors, and computing the promises to fulfill.
 *
 * @param curr The current action, as passed from the user context; may be
 * freed/invalidated after the execution of this function, with a different
 * action "returned" its place (pass-by-reference)
 * @return True if curr is a newly-explored action; false otherwise
 */
bool ModelExecution::initialize_curr_action(ModelAction **curr)
{
	ModelAction *newcurr;

	if ((*curr)->is_rmwc() || (*curr)->is_rmw()) {
		newcurr = process_rmw(*curr);
		delete *curr;

		*curr = newcurr;
		return false;
	}

	(*curr)->set_seq_number(get_next_seq_num());

	newcurr = node_stack->explore_action(*curr);
	if (newcurr) {
		/* First restore type and order in case of RMW operation */
		if ((*curr)->is_rmwr())
			newcurr->copy_typeandorder(*curr);

		ASSERT((*curr)->get_location() == newcurr->get_location());
		newcurr->copy_from_new(*curr);

		/* Discard duplicate ModelAction; use action from NodeStack */
		delete *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		*curr = newcurr;
		return false;							/* Action was explored previously */
	} else {
		newcurr = *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		/* Assign most recent release fence */
		newcurr->set_last_fence_release(get_last_fence_release(newcurr->get_tid()));

		return true;						/* This was a new ModelAction */
	}
}

/**
 * @brief Establish reads-from relation between two actions
 *
 * Perform basic operations involved with establishing a concrete rf relation,
 * including setting the ModelAction data and checking for release sequences.
 *
 * @param act The action that is reading (must be a read)
 * @param rf The action from which we are reading (must be a write)
 *
 * @return True if this read established synchronization
 */

bool ModelExecution::read_from(ModelAction *act, const ModelAction *rf)
{
	ASSERT(rf);
	ASSERT(rf->is_write());

	act->set_read_from(rf);
	if (act->is_acquire()) {
		rel_heads_list_t release_heads;
		get_release_seq_heads(act, act, &release_heads);
		int num_heads = release_heads.size();
		for (unsigned int i = 0;i < release_heads.size();i++)
			if (!synchronize(release_heads[i], act))
				num_heads--;
		return num_heads > 0;
	}
	return false;
}

/**
 * @brief Synchronizes two actions
 *
 * When A synchronizes with B (or A --sw-> B), B inherits A's clock vector.
 * This function performs the synchronization as well as providing other hooks
 * for other checks along with synchronization.
 *
 * @param first The left-hand side of the synchronizes-with relation
 * @param second The right-hand side of the synchronizes-with relation
 * @return True if the synchronization was successful (i.e., was consistent
 * with the execution order); false otherwise
 */
bool ModelExecution::synchronize(const ModelAction *first, ModelAction *second)
{
	if (*second < *first) {
		set_bad_synchronization();
		return false;
	}
	return second->synchronize_with(first);
}

/**
 * @brief Check whether a model action is enabled.
 *
 * Checks whether an operation would be successful (i.e., is a lock already
 * locked, or is the joined thread already complete).
 *
 * For yield-blocking, yields are never enabled.
 *
 * @param curr is the ModelAction to check whether it is enabled.
 * @return a bool that indicates whether the action is enabled.
 */
bool ModelExecution::check_action_enabled(ModelAction *curr) {
	if (curr->is_lock()) {
		cdsc::mutex *lock = curr->get_mutex();
		struct cdsc::mutex_state *state = lock->get_state();
		if (state->locked)
			return false;
	} else if (curr->is_thread_join()) {
		Thread *blocking = curr->get_thread_operand();
		if (!blocking->is_complete()) {
			return false;
		}
	}

	return true;
}

/**
 * This is the heart of the model checker routine. It performs model-checking
 * actions corresponding to a given "current action." Among other processes, it
 * calculates reads-from relationships, updates synchronization clock vectors,
 * forms a memory_order constraints graph, and handles replay/backtrack
 * execution when running permutations of previously-observed executions.
 *
 * @param curr The current action to process
 * @return The ModelAction that is actually executed; may be different than
 * curr
 */
ModelAction * ModelExecution::check_current_action(ModelAction *curr)
{
	ASSERT(curr);
	bool second_part_of_rmw = curr->is_rmwc() || curr->is_rmw();
	bool newly_explored = initialize_curr_action(&curr);

	DBG();

	wake_up_sleeping_actions(curr);

	/* Add the action to lists before any other model-checking tasks */
	if (!second_part_of_rmw)
		add_action_to_lists(curr);

	ModelVector<ModelAction *> * rf_set = NULL;
	/* Build may_read_from set for newly-created actions */
	if (newly_explored && curr->is_read())
		rf_set = build_may_read_from(curr);

	process_thread_action(curr);

	if (curr->is_read() && !second_part_of_rmw) {
		process_read(curr, rf_set);
		delete rf_set;
	} else {
		ASSERT(rf_set == NULL);
	}

	if (curr->is_write())
		process_write(curr);

	if (curr->is_fence())
		process_fence(curr);

	if (curr->is_mutex_op())
		process_mutex(curr);

	return curr;
}

/**
 * This is the strongest feasibility check available.
 * @return whether the current trace (partial or complete) must be a prefix of
 * a feasible trace.
 */
bool ModelExecution::isfeasibleprefix() const
{
	return !is_infeasible();
}

/**
 * Print disagnostic information about an infeasible execution
 * @param prefix A string to prefix the output with; if NULL, then a default
 * message prefix will be provided
 */
void ModelExecution::print_infeasibility(const char *prefix) const
{
	char buf[100];
	char *ptr = buf;
	if (mo_graph->checkForCycles())
		ptr += sprintf(ptr, "[mo cycle]");
	if (priv->bad_synchronization)
		ptr += sprintf(ptr, "[bad sw ordering]");
	if (priv->bad_sc_read)
		ptr += sprintf(ptr, "[bad sc read]");
	if (ptr != buf)
		model_print("%s: %s", prefix ? prefix : "Infeasible", buf);
}

/**
 * Check if the current partial trace is infeasible. Does not check any
 * end-of-execution flags, which might rule out the execution. Thus, this is
 * useful only for ruling an execution as infeasible.
 * @return whether the current partial trace is infeasible.
 */
bool ModelExecution::is_infeasible() const
{
	return mo_graph->checkForCycles() ||
				 priv->bad_synchronization ||
				 priv->bad_sc_read;
}

/** Close out a RMWR by converting previous RMWR into a RMW or READ. */
ModelAction * ModelExecution::process_rmw(ModelAction *act) {
	ModelAction *lastread = get_last_action(act->get_tid());
	lastread->process_rmw(act);
	if (act->is_rmw()) {
		mo_graph->addRMWEdge(lastread->get_reads_from(), lastread);
		mo_graph->commitChanges();
	}
	return lastread;
}

/**
 * @brief Updates the mo_graph with the constraints imposed from the current
 * read.
 *
 * Basic idea is the following: Go through each other thread and find
 * the last action that happened before our read.  Two cases:
 *
 * -# The action is a write: that write must either occur before
 * the write we read from or be the write we read from.
 * -# The action is a read: the write that that action read from
 * must occur before the write we read from or be the same write.
 *
 * @param curr The current action. Must be a read.
 * @param rf The ModelAction or Promise that curr reads from. Must be a write.
 * @return True if modification order edges were added; false otherwise
 */
template <typename rf_type>
bool ModelExecution::r_modification_order(ModelAction *curr, const rf_type *rf)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_read());

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);
	ModelAction *last_sc_write = NULL;
	if (curr->is_seqcst())
		last_sc_write = get_last_seq_cst_write(curr);

	/* Iterate over all threads */
	for (i = 0;i < thrd_lists->size();i++) {
		/* Last SC fence in thread i */
		ModelAction *last_sc_fence_thread_local = NULL;
		if (int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_local = get_last_seq_cst_fence(int_to_id(i), NULL);

		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local)
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin();rit != list->rend();rit++) {
			ModelAction *act = *rit;

			/* Skip curr */
			if (act == curr)
				continue;
			/* Don't want to add reflexive edges on 'rf' */
			if (act->equals(rf)) {
				if (act->happens_before(curr))
					break;
				else
					continue;
			}

			if (act->is_write()) {
				/* C++, Section 29.3 statement 5 */
				if (curr->is_seqcst() && last_sc_fence_thread_local &&
						*act < *last_sc_fence_thread_local) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
				/* C++, Section 29.3 statement 4 */
				else if (act->is_seqcst() && last_sc_fence_local &&
								 *act < *last_sc_fence_local) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
				/* C++, Section 29.3 statement 6 */
				else if (last_sc_fence_thread_before &&
								 *act < *last_sc_fence_thread_before) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr
			 */
			if (act->happens_before(curr)) {
				if (act->is_write()) {
					added = mo_graph->addEdge(act, rf) || added;
				} else {
					const ModelAction *prevrf = act->get_reads_from();
					if (!prevrf->equals(rf))
						added = mo_graph->addEdge(prevrf, rf) || added;
				}
				break;
			}
		}
	}

	return added;
}

/**
 * Updates the mo_graph with the constraints imposed from the current write.
 *
 * Basic idea is the following: Go through each other thread and find
 * the lastest action that happened before our write.  Two cases:
 *
 * (1) The action is a write => that write must occur before
 * the current write
 *
 * (2) The action is a read => the write that that action read from
 * must occur before the current write.
 *
 * This method also handles two other issues:
 *
 * (I) Sequential Consistency: Making sure that if the current write is
 * seq_cst, that it occurs after the previous seq_cst write.
 *
 * (II) Sending the write back to non-synchronizing reads.
 *
 * @param curr The current action. Must be a write.
 * @param send_fv A vector for stashing reads to which we may pass our future
 * value. If NULL, then don't record any future values.
 * @return True if modification order edges were added; false otherwise
 */
bool ModelExecution::w_modification_order(ModelAction *curr)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_write());

	if (curr->is_seqcst()) {
		/* We have to at least see the last sequentially consistent write,
		         so we are initialized. */
		ModelAction *last_seq_cst = get_last_seq_cst_write(curr);
		if (last_seq_cst != NULL) {
			added = mo_graph->addEdge(last_seq_cst, curr) || added;
		}
	}

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);

	/* Iterate over all threads */
	for (i = 0;i < thrd_lists->size();i++) {
		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local && int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin();rit != list->rend();rit++) {
			ModelAction *act = *rit;
			if (act == curr) {
				/*
				 * 1) If RMW and it actually read from something, then we
				 * already have all relevant edges, so just skip to next
				 * thread.
				 *
				 * 2) If RMW and it didn't read from anything, we should
				 * whatever edge we can get to speed up convergence.
				 *
				 * 3) If normal write, we need to look at earlier actions, so
				 * continue processing list.
				 */
				if (curr->is_rmw()) {
					if (curr->get_reads_from() != NULL)
						break;
					else
						continue;
				} else
					continue;
			}

			/* C++, Section 29.3 statement 7 */
			if (last_sc_fence_thread_before && act->is_write() &&
					*act < *last_sc_fence_thread_before) {
				added = mo_graph->addEdge(act, curr) || added;
				break;
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr
			 */
			if (act->happens_before(curr)) {
				/*
				 * Note: if act is RMW, just add edge:
				 *   act --mo--> curr
				 * The following edge should be handled elsewhere:
				 *   readfrom(act) --mo--> act
				 */
				if (act->is_write())
					added = mo_graph->addEdge(act, curr) || added;
				else if (act->is_read()) {
					//if previous read accessed a null, just keep going
					added = mo_graph->addEdge(act->get_reads_from(), curr) || added;
				}
				break;
			} else if (act->is_read() && !act->could_synchronize_with(curr) &&
								 !act->same_thread(curr)) {
				/* We have an action that:
				   (1) did not happen before us
				   (2) is a read and we are a write
				   (3) cannot synchronize with us
				   (4) is in a different thread
				   =>
				   that read could potentially read from our write.  Note that
				   these checks are overly conservative at this point, we'll
				   do more checks before actually removing the
				   pendingfuturevalue.

				 */

			}
		}
	}

	return added;
}

/**
 * Arbitrary reads from the future are not allowed. Section 29.3 part 9 places
 * some constraints. This method checks one the following constraint (others
 * require compiler support):
 *
 *   If X --hb-> Y --mo-> Z, then X should not read from Z.
 *   If X --hb-> Y, A --rf-> Y, and A --mo-> Z, then X should not read from Z.
 */
bool ModelExecution::mo_may_allow(const ModelAction *writer, const ModelAction *reader)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(reader->get_location());
	unsigned int i;
	/* Iterate over all threads */
	for (i = 0;i < thrd_lists->size();i++) {
		const ModelAction *write_after_read = NULL;

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin();rit != list->rend();rit++) {
			ModelAction *act = *rit;

			/* Don't disallow due to act == reader */
			if (!reader->happens_before(act) || reader == act)
				break;
			else if (act->is_write())
				write_after_read = act;
			else if (act->is_read() && act->get_reads_from() != NULL)
				write_after_read = act->get_reads_from();
		}

		if (write_after_read && write_after_read != writer && mo_graph->checkReachable(write_after_read, writer))
			return false;
	}
	return true;
}

/**
 * Finds the head(s) of the release sequence(s) containing a given ModelAction.
 * The ModelAction under consideration is expected to be taking part in
 * release/acquire synchronization as an object of the "reads from" relation.
 * Note that this can only provide release sequence support for RMW chains
 * which do not read from the future, as those actions cannot be traced until
 * their "promise" is fulfilled. Similarly, we may not even establish the
 * presence of a release sequence with certainty, as some modification order
 * constraints may be decided further in the future. Thus, this function
 * "returns" two pieces of data: a pass-by-reference vector of @a release_heads
 * and a boolean representing certainty.
 *
 * @param rf The action that might be part of a release sequence. Must be a
 * write.
 * @param release_heads A pass-by-reference style return parameter. After
 * execution of this function, release_heads will contain the heads of all the
 * relevant release sequences, if any exists with certainty
 * @return true, if the ModelExecution is certain that release_heads is complete;
 * false otherwise
 */
bool ModelExecution::release_seq_heads(const ModelAction *rf,
																			 rel_heads_list_t *release_heads) const
{
	/* Only check for release sequences if there are no cycles */
	if (mo_graph->checkForCycles())
		return false;

	for ( ;rf != NULL;rf = rf->get_reads_from()) {
		ASSERT(rf->is_write());

		if (rf->is_release())
			release_heads->push_back(rf);
		else if (rf->get_last_fence_release())
			release_heads->push_back(rf->get_last_fence_release());
		if (!rf->is_rmw())
			break;									/* End of RMW chain */

		/** @todo Need to be smarter here...  In the linux lock
		 * example, this will run to the beginning of the program for
		 * every acquire. */
		/** @todo The way to be smarter here is to keep going until 1
		 * thread has a release preceded by an acquire and you've seen
		 *	 both. */

		/* acq_rel RMW is a sufficient stopping condition */
		if (rf->is_acquire() && rf->is_release())
			return true;									/* complete */
	};
	ASSERT(rf);				// Needs to be real write

	if (rf->is_release())
		return true;						/* complete */

	/* else relaxed write
	 * - check for fence-release in the same thread (29.8, stmt. 3)
	 * - check modification order for contiguous subsequence
	 *   -> rf must be same thread as release */

	const ModelAction *fence_release = rf->get_last_fence_release();
	/* Synchronize with a fence-release unconditionally; we don't need to
	 * find any more "contiguous subsequence..." for it */
	if (fence_release)
		release_heads->push_back(fence_release);

	return true;			/* complete */
}

/**
 * An interface for getting the release sequence head(s) with which a
 * given ModelAction must synchronize. This function only returns a non-empty
 * result when it can locate a release sequence head with certainty. Otherwise,
 * it may mark the internal state of the ModelExecution so that it will handle
 * the release sequence at a later time, causing @a acquire to update its
 * synchronization at some later point in execution.
 *
 * @param acquire The 'acquire' action that may synchronize with a release
 * sequence
 * @param read The read action that may read from a release sequence; this may
 * be the same as acquire, or else an earlier action in the same thread (i.e.,
 * when 'acquire' is a fence-acquire)
 * @param release_heads A pass-by-reference return parameter. Will be filled
 * with the head(s) of the release sequence(s), if they exists with certainty.
 * @see ModelExecution::release_seq_heads
 */
void ModelExecution::get_release_seq_heads(ModelAction *acquire,
																					 ModelAction *read, rel_heads_list_t *release_heads)
{
	const ModelAction *rf = read->get_reads_from();

	release_seq_heads(rf, release_heads);
}

/**
 * Performs various bookkeeping operations for the current ModelAction. For
 * instance, adds action to the per-object, per-thread action vector and to the
 * action trace list of all thread actions.
 *
 * @param act is the ModelAction to add.
 */
void ModelExecution::add_action_to_lists(ModelAction *act)
{
	int tid = id_to_int(act->get_tid());
	ModelAction *uninit = NULL;
	int uninit_id = -1;
	action_list_t *list = get_safe_ptr_action(&obj_map, act->get_location());
	if (list->empty() && act->is_atomic_var()) {
		uninit = get_uninitialized_action(act);
		uninit_id = id_to_int(uninit->get_tid());
		list->push_front(uninit);
	}
	list->push_back(act);

	action_trace.push_back(act);
	if (uninit)
		action_trace.push_front(uninit);

	SnapVector<action_list_t> *vec = get_safe_ptr_vect_action(&obj_thrd_map, act->get_location());
	if (tid >= (int)vec->size())
		vec->resize(priv->next_thread_id);
	(*vec)[tid].push_back(act);
	if (uninit)
		(*vec)[uninit_id].push_front(uninit);

	if ((int)thrd_last_action.size() <= tid)
		thrd_last_action.resize(get_num_threads());
	thrd_last_action[tid] = act;
	if (uninit)
		thrd_last_action[uninit_id] = uninit;

	if (act->is_fence() && act->is_release()) {
		if ((int)thrd_last_fence_release.size() <= tid)
			thrd_last_fence_release.resize(get_num_threads());
		thrd_last_fence_release[tid] = act;
	}

	if (act->is_wait()) {
		void *mutex_loc = (void *) act->get_value();
		get_safe_ptr_action(&obj_map, mutex_loc)->push_back(act);

		SnapVector<action_list_t> *vec = get_safe_ptr_vect_action(&obj_thrd_map, mutex_loc);
		if (tid >= (int)vec->size())
			vec->resize(priv->next_thread_id);
		(*vec)[tid].push_back(act);
	}
}

/**
 * @brief Get the last action performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last action in the thread
 */
ModelAction * ModelExecution::get_last_action(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_action.size())
		return thrd_last_action[id_to_int(tid)];
	else
		return NULL;
}

/**
 * @brief Get the last fence release performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last fence release in the thread, if one exists; NULL otherwise
 */
ModelAction * ModelExecution::get_last_fence_release(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_fence_release.size())
		return thrd_last_fence_release[id_to_int(tid)];
	else
		return NULL;
}

/**
 * Gets the last memory_order_seq_cst write (in the total global sequence)
 * performed on a particular object (i.e., memory location), not including the
 * current action.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last seq_cst write
 */
ModelAction * ModelExecution::get_last_seq_cst_write(ModelAction *curr) const
{
	void *location = curr->get_location();
	action_list_t *list = obj_map.get(location);
	/* Find: max({i in dom(S) | seq_cst(t_i) && isWrite(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin();(*rit) != curr;rit++)
		;
	rit++;			/* Skip past curr */
	for ( ;rit != list->rend();rit++)
		if ((*rit)->is_write() && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

/**
 * Gets the last memory_order_seq_cst fence (in the total global sequence)
 * performed in a particular thread, prior to a particular fence.
 * @param tid The ID of the thread to check
 * @param before_fence The fence from which to begin the search; if NULL, then
 * search for the most recent fence in the thread.
 * @return The last prior seq_cst fence in the thread, if exists; otherwise, NULL
 */
ModelAction * ModelExecution::get_last_seq_cst_fence(thread_id_t tid, const ModelAction *before_fence) const
{
	/* All fences should have location FENCE_LOCATION */
	action_list_t *list = obj_map.get(FENCE_LOCATION);

	if (!list)
		return NULL;

	action_list_t::reverse_iterator rit = list->rbegin();

	if (before_fence) {
		for (;rit != list->rend();rit++)
			if (*rit == before_fence)
				break;

		ASSERT(*rit == before_fence);
		rit++;
	}

	for (;rit != list->rend();rit++)
		if ((*rit)->is_fence() && (tid == (*rit)->get_tid()) && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

/**
 * Gets the last unlock operation performed on a particular mutex (i.e., memory
 * location). This function identifies the mutex according to the current
 * action, which is presumed to perform on the same mutex.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last unlock operation
 */
ModelAction * ModelExecution::get_last_unlock(ModelAction *curr) const
{
	void *location = curr->get_location();

	action_list_t *list = obj_map.get(location);
	/* Find: max({i in dom(S) | isUnlock(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin();rit != list->rend();rit++)
		if ((*rit)->is_unlock() || (*rit)->is_wait())
			return *rit;
	return NULL;
}

ModelAction * ModelExecution::get_parent_action(thread_id_t tid) const
{
	ModelAction *parent = get_last_action(tid);
	if (!parent)
		parent = get_thread(tid)->get_creation();
	return parent;
}

/**
 * Returns the clock vector for a given thread.
 * @param tid The thread whose clock vector we want
 * @return Desired clock vector
 */
ClockVector * ModelExecution::get_cv(thread_id_t tid) const
{
	return get_parent_action(tid)->get_cv();
}

bool valequals(uint64_t val1, uint64_t val2, int size) {
	switch(size) {
	case 1:
		return ((uint8_t)val1) == ((uint8_t)val2);
	case 2:
		return ((uint16_t)val1) == ((uint16_t)val2);
	case 4:
		return ((uint32_t)val1) == ((uint32_t)val2);
	case 8:
		return val1==val2;
	default:
		ASSERT(0);
		return false;
	}
}

/**
 * Build up an initial set of all past writes that this 'read' action may read
 * from, as well as any previously-observed future values that must still be valid.
 *
 * @param curr is the current ModelAction that we are exploring; it must be a
 * 'read' operation.
 */
ModelVector<ModelAction *> *  ModelExecution::build_may_read_from(ModelAction *curr)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	ModelAction *last_sc_write = NULL;

	if (curr->is_seqcst())
		last_sc_write = get_last_seq_cst_write(curr);

	ModelVector<ModelAction *> * rf_set = new ModelVector<ModelAction *>();

	/* Iterate over all threads */
	for (i = 0;i < thrd_lists->size();i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin();rit != list->rend();rit++) {
			ModelAction *act = *rit;

			/* Only consider 'write' actions */
			if (!act->is_write() || act == curr)
				continue;

			/* Don't consider more than one seq_cst write if we are a seq_cst read. */
			bool allow_read = true;

			if (curr->is_seqcst() && (act->is_seqcst() || (last_sc_write != NULL && act->happens_before(last_sc_write))) && act != last_sc_write)
				allow_read = false;

			/* Need to check whether we will have two RMW reading from the same value */
			if (curr->is_rmwr()) {
				/* It is okay if we have a failing CAS */
				if (!curr->is_rmwrcas() ||
						valequals(curr->get_value(), act->get_value(), curr->getSize())) {
					//Need to make sure we aren't the second RMW
					CycleNode * node = mo_graph->getNode_noCreate(act);
					if (node != NULL && node->getRMW() != NULL) {
						//we are the second RMW
						allow_read = false;
					}
				}
			}

			if (allow_read) {
				/* Only add feasible reads */
				mo_graph->startChanges();
				r_modification_order(curr, act);
				if (!is_infeasible())
					rf_set->push_back(act);
				mo_graph->rollbackChanges();
			}

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr))
				break;
		}
	}

	if (DBG_ENABLED()) {
		model_print("Reached read action:\n");
		curr->print();
		model_print("End printing read_from_past\n");
	}
	return rf_set;
}

/**
 * @brief Get an action representing an uninitialized atomic
 *
 * This function may create a new one or try to retrieve one from the NodeStack
 *
 * @param curr The current action, which prompts the creation of an UNINIT action
 * @return A pointer to the UNINIT ModelAction
 */
ModelAction * ModelExecution::get_uninitialized_action(const ModelAction *curr) const
{
	Node *node = curr->get_node();
	ModelAction *act = node->get_uninit_action();
	if (!act) {
		act = new ModelAction(ATOMIC_UNINIT, std::memory_order_relaxed, curr->get_location(), params->uninitvalue, model_thread);
		node->set_uninit_action(act);
	}
	act->create_cv(NULL);
	return act;
}

static void print_list(const action_list_t *list)
{
	action_list_t::const_iterator it;

	model_print("------------------------------------------------------------------------------------\n");
	model_print("#    t    Action type     MO       Location         Value               Rf  CV\n");
	model_print("------------------------------------------------------------------------------------\n");

	unsigned int hash = 0;

	for (it = list->begin();it != list->end();it++) {
		const ModelAction *act = *it;
		if (act->get_seq_number() > 0)
			act->print();
		hash = hash^(hash<<3)^((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("------------------------------------------------------------------------------------\n");
}

#if SUPPORT_MOD_ORDER_DUMP
void ModelExecution::dumpGraph(char *filename) const
{
	char buffer[200];
	sprintf(buffer, "%s.dot", filename);
	FILE *file = fopen(buffer, "w");
	fprintf(file, "digraph %s {\n", filename);
	mo_graph->dumpNodes(file);
	ModelAction **thread_array = (ModelAction **)model_calloc(1, sizeof(ModelAction *) * get_num_threads());

	for (action_list_t::const_iterator it = action_trace.begin();it != action_trace.end();it++) {
		ModelAction *act = *it;
		if (act->is_read()) {
			mo_graph->dot_print_node(file, act);
			mo_graph->dot_print_edge(file,
															 act->get_reads_from(),
															 act,
															 "label=\"rf\", color=red, weight=2");
		}
		if (thread_array[act->get_tid()]) {
			mo_graph->dot_print_edge(file,
															 thread_array[id_to_int(act->get_tid())],
															 act,
															 "label=\"sb\", color=blue, weight=400");
		}

		thread_array[act->get_tid()] = act;
	}
	fprintf(file, "}\n");
	model_free(thread_array);
	fclose(file);
}
#endif

/** @brief Prints an execution trace summary. */
void ModelExecution::print_summary() const
{
#if SUPPORT_MOD_ORDER_DUMP
	char buffername[100];
	sprintf(buffername, "exec%04u", get_execution_number());
	mo_graph->dumpGraphToFile(buffername);
	sprintf(buffername, "graph%04u", get_execution_number());
	dumpGraph(buffername);
#endif

	model_print("Execution trace %d:", get_execution_number());
	if (isfeasibleprefix()) {
		if (scheduler->all_threads_sleeping())
			model_print(" SLEEP-SET REDUNDANT");
		if (have_bug_reports())
			model_print(" DETECTED BUG(S)");
	} else
		print_infeasibility(" INFEASIBLE");
	model_print("\n");

	print_list(&action_trace);
	model_print("\n");

}

/**
 * Add a Thread to the system for the first time. Should only be called once
 * per thread.
 * @param t The Thread to add
 */
void ModelExecution::add_thread(Thread *t)
{
	unsigned int i = id_to_int(t->get_id());
	if (i >= thread_map.size())
		thread_map.resize(i + 1);
	thread_map[i] = t;
	if (!t->is_model_thread())
		scheduler->add_thread(t);
}

/**
 * @brief Get a Thread reference by its ID
 * @param tid The Thread's ID
 * @return A Thread reference
 */
Thread * ModelExecution::get_thread(thread_id_t tid) const
{
	unsigned int i = id_to_int(tid);
	if (i < thread_map.size())
		return thread_map[i];
	return NULL;
}

/**
 * @brief Get a reference to the Thread in which a ModelAction was executed
 * @param act The ModelAction
 * @return A Thread reference
 */
Thread * ModelExecution::get_thread(const ModelAction *act) const
{
	return get_thread(act->get_tid());
}

/**
 * @brief Get a Thread reference by its pthread ID
 * @param index The pthread's ID
 * @return A Thread reference
 */
Thread * ModelExecution::get_pthread(pthread_t pid) {
	union {
		pthread_t p;
		uint32_t v;
	} x;
	x.p = pid;
	uint32_t thread_id = x.v;
	if (thread_id < pthread_counter + 1) return pthread_map[thread_id];
	else return NULL;
}

/**
 * @brief Check if a Thread is currently enabled
 * @param t The Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelExecution::is_enabled(Thread *t) const
{
	return scheduler->is_enabled(t);
}

/**
 * @brief Check if a Thread is currently enabled
 * @param tid The ID of the Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelExecution::is_enabled(thread_id_t tid) const
{
	return scheduler->is_enabled(tid);
}

/**
 * @brief Select the next thread to execute based on the curren action
 *
 * RMW actions occur in two parts, and we cannot split them. And THREAD_CREATE
 * actions should be followed by the execution of their child thread. In either
 * case, the current action should determine the next thread schedule.
 *
 * @param curr The current action
 * @return The next thread to run, if the current action will determine this
 * selection; otherwise NULL
 */
Thread * ModelExecution::action_select_next_thread(const ModelAction *curr) const
{
	/* Do not split atomic RMW */
	if (curr->is_rmwr())
		return get_thread(curr);
	if (curr->is_write()) {
		std::memory_order order = curr->get_mo();
		switch(order) {
		case std::memory_order_relaxed:
			return get_thread(curr);
		case std::memory_order_release:
			return get_thread(curr);
		default:
			return NULL;
		}
	}

	/* Follow CREATE with the created thread */
	/* which is not needed, because model.cc takes care of this */
	if (curr->get_type() == THREAD_CREATE)
		return curr->get_thread_operand();
	if (curr->get_type() == PTHREAD_CREATE) {
		return curr->get_thread_operand();
	}
	return NULL;
}

/**
 * Takes the next step in the execution, if possible.
 * @param curr The current step to take
 * @return Returns the next Thread to run, if any; NULL if this execution
 * should terminate
 */
Thread * ModelExecution::take_step(ModelAction *curr)
{
	Thread *curr_thrd = get_thread(curr);
	ASSERT(curr_thrd->get_state() == THREAD_READY);

	ASSERT(check_action_enabled(curr));				/* May have side effects? */
	curr = check_current_action(curr);
	ASSERT(curr);

	if (curr_thrd->is_blocked() || curr_thrd->is_complete())
		scheduler->remove_thread(curr_thrd);

	return action_select_next_thread(curr);
}

Fuzzer * ModelExecution::getFuzzer() {
	return fuzzer;
}
