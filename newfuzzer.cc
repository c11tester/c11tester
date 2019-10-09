#include "newfuzzer.h"
#include "threads-model.h"
#include "action.h"
#include "history.h"
#include "funcnode.h"
#include "funcinst.h"
#include "predicate.h"
#include "concretepredicate.h"
#include "waitobj.h"

#include "model.h"
#include "schedule.h"
#include "execution.h"

NewFuzzer::NewFuzzer() :
	thrd_last_read_act(),
	thrd_last_func_inst(),
	thrd_selected_child_branch(),
	thrd_pruned_writes(),
	paused_thread_list(),
	paused_thread_table(128),
	failed_predicates(32)
{}

/**
 * @brief Register the ModelHistory and ModelExecution engine
 */
void NewFuzzer::register_engine(ModelHistory * history, ModelExecution *execution)
{
	this->history = history;
	this->execution = execution;
}

int NewFuzzer::selectWrite(ModelAction *read, SnapVector<ModelAction *> * rf_set)
{
//	return random() % rf_set->size();

	thread_id_t tid = read->get_tid();
	int thread_id = id_to_int(tid);

	if (thrd_last_read_act.size() <= (uint) thread_id) {
		thrd_last_read_act.resize(thread_id + 1);
		thrd_last_func_inst.resize(thread_id + 1);
	}

	// A new read action is encountered, select a random child branch of current predicate
	if (read != thrd_last_read_act[thread_id]) {
		FuncNode * func_node = history->get_curr_func_node(tid);
		Predicate * curr_pred = func_node->get_predicate_tree_position(tid);
		FuncInst * read_inst = func_node->get_inst(read);
		Predicate * selected_branch = selectBranch(tid, curr_pred, read_inst);

		inst_act_map_t * inst_act_map = func_node->get_inst_act_map(tid);
		prune_writes(tid, selected_branch, rf_set, inst_act_map);

		if (!failed_predicates.isEmpty())
			failed_predicates.reset();

		thrd_last_read_act[thread_id] = read;
		thrd_last_func_inst[thread_id] = read_inst;
	}

	// No write satisfies the selected predicate, so pause this thread.
	while ( rf_set->size() == 0 ) {
		Thread * read_thread = execution->get_thread(tid);
		model_print("the %d read action of thread %d at %p is unsuccessful\n", read->get_seq_number(), read_thread->get_id(), read->get_location());

		if (find_threads(read)) {
			// reset thread pending action and revert sequence numbers
			read_thread->set_pending(read);
			read->reset_seq_number();
			execution->restore_last_seq_num();

			conditional_sleep(read_thread);

			// Returning -1 stops the while loop of ModelExecution::process_read
			return -1;
		} else {
			Predicate * selected_branch = get_selected_child_branch(tid);
			failed_predicates.put(selected_branch, true);

			SnapVector<ModelAction *> * pruned_writes = thrd_pruned_writes[thread_id];
			for (uint i = 0; i < pruned_writes->size(); i++) {
				rf_set->push_back( (*pruned_writes)[i] );
			}

			// Reselect a predicate and prune writes
			Predicate * curr_pred = selected_branch->get_parent();
			FuncInst * read_inst = thrd_last_func_inst[thread_id];
			selected_branch = selectBranch(tid, curr_pred, read_inst);

			FuncNode * func_node = history->get_curr_func_node(tid);
			inst_act_map_t * inst_act_map = func_node->get_inst_act_map(tid);
			prune_writes(tid, selected_branch, rf_set, inst_act_map);

			ASSERT(selected_branch);
		}
	}

	ASSERT(rf_set->size() != 0);
	int random_index = random() % rf_set->size();

	return random_index;
}

/* Select a random branch from the children of curr_pred 
 * @return The selected branch
 */
Predicate * NewFuzzer::selectBranch(thread_id_t tid, Predicate * curr_pred, FuncInst * read_inst)
{
	int thread_id = id_to_int(tid);
	if ( thrd_selected_child_branch.size() <= (uint) thread_id)
		thrd_selected_child_branch.resize(thread_id + 1);

	if (curr_pred == NULL || read_inst == NULL) {
		thrd_selected_child_branch[thread_id] = NULL;
		return NULL;
	}

	ModelVector<Predicate *> * children = curr_pred->get_children();
	SnapVector<Predicate *> branches;

	for (uint i = 0; i < children->size(); i++) {
		Predicate * child = (*children)[i];
		if (child->get_func_inst() == read_inst && !failed_predicates.contains(child)) {
			branches.push_back(child);
		}
	}

	// predicate children have not been generated
	if (branches.size() == 0) {
		thrd_selected_child_branch[thread_id] = NULL;
		return NULL;
	}

	// randomly select a branch
	int random_index = random() % branches.size();
	Predicate * random_branch = branches[ random_index ];
	thrd_selected_child_branch[thread_id] = random_branch;

	return random_branch;
}

Predicate * NewFuzzer::get_selected_child_branch(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	if (thrd_selected_child_branch.size() <= (uint) thread_id)
		return NULL;

	return thrd_selected_child_branch[thread_id];
}

/* Remove writes from the rf_set that do not satisfie the selected predicate, 
 * and store them in thrd_pruned_writes
 *
 * @return true if rf_set is pruned
 */
bool NewFuzzer::prune_writes(thread_id_t tid, Predicate * pred,
	SnapVector<ModelAction *> * rf_set, inst_act_map_t * inst_act_map)
{
	if (pred == NULL)
		return false;

	PredExprSet * pred_expressions = pred->get_pred_expressions();
	if (pred_expressions->getSize() == 0)	// unset predicates
		return false;

	int thread_id = id_to_int(tid);
	uint old_size = thrd_pruned_writes.size();
	if (thrd_pruned_writes.size() <= (uint) thread_id) {
		uint new_size = thread_id + 1;
		thrd_pruned_writes.resize(new_size);
		for (uint i = old_size; i < new_size; i++)
			thrd_pruned_writes[i] = new SnapVector<ModelAction *>();
	}
	SnapVector<ModelAction *> * pruned_writes = thrd_pruned_writes[thread_id];
	pruned_writes->clear();	// clear the old pruned_writes set

	bool pruned = false;
	uint index = 0;

	ConcretePredicate * concrete_pred = pred->evaluate(inst_act_map, tid);
	SnapVector<struct concrete_pred_expr> * concrete_exprs = concrete_pred->getExpressions();

	while ( index < rf_set->size() ) {
		ModelAction * write_act = (*rf_set)[index];
		uint64_t write_val = write_act->get_write_value();
		bool satisfy_predicate = true;

		for (uint i = 0; i < concrete_exprs->size(); i++) {
			struct concrete_pred_expr concrete = (*concrete_exprs)[i];
			bool equality;

			switch (concrete.token) {
				case NOPREDICATE:
					return false;
				case EQUALITY:
					equality = (write_val == concrete.value);
					if (equality != concrete.equality)
						satisfy_predicate = false;
					break;
				case NULLITY:
					equality = ((void*)write_val == NULL);
                                        if (equality != concrete.equality)
                                                satisfy_predicate = false;
                                        break;
				default:
					model_print("unknown predicate token\n");
					break;
			}

			if (!satisfy_predicate)
				break;
		}

		if (!satisfy_predicate) {
			ASSERT(rf_set != NULL);
			(*rf_set)[index] = rf_set->back();
			rf_set->pop_back();
			pruned_writes->push_back(write_act);
			pruned = true;
		} else
			index++;
	}

	delete concrete_pred;

	return pruned;
}

/* @brief Put a thread to sleep because no writes in rf_set satisfies the selected predicate. 
 *
 * @param thread A thread whose last action is a read
 */
void NewFuzzer::conditional_sleep(Thread * thread)
{
	int index = paused_thread_list.size();

	model->getScheduler()->add_sleep(thread);
	paused_thread_list.push_back(thread);
	paused_thread_table.put(thread, index);	// Update table

	/* Add the waiting condition to ModelHistory */
	ModelAction * read = thread->get_pending();
	thread_id_t tid = thread->get_id();
	FuncNode * func_node = history->get_curr_func_node(tid);
	inst_act_map_t * inst_act_map = func_node->get_inst_act_map(tid);

	Predicate * selected_branch = get_selected_child_branch(tid);
	ConcretePredicate * concrete = selected_branch->evaluate(inst_act_map, tid);
	concrete->set_location(read->get_location());

	history->add_waiting_write(concrete);
	/* history->add_waiting_thread is already called in find_threads */
}

bool NewFuzzer::has_paused_threads()
{
	return paused_thread_list.size() != 0;
}

Thread * NewFuzzer::selectThread(int * threadlist, int numthreads)
{
	if (numthreads == 0 && has_paused_threads()) {
		wake_up_paused_threads(threadlist, &numthreads);
		//model_print("list size: %d, active t id: %d\n", numthreads, threadlist[0]);
	}

	int random_index = random() % numthreads;
	int thread = threadlist[random_index];
	thread_id_t curr_tid = int_to_id(thread);
	return execution->get_thread(curr_tid);
}

/* Force waking up one of threads paused by Fuzzer, because otherwise
 * the Fuzzer is not making progress
 */
void NewFuzzer::wake_up_paused_threads(int * threadlist, int * numthreads)
{
	int random_index = random() % paused_thread_list.size();
	Thread * thread = paused_thread_list[random_index];
	model->getScheduler()->remove_sleep(thread);

	Thread * last_thread = paused_thread_list.back();
	paused_thread_list[random_index] = last_thread;
	paused_thread_list.pop_back();
	paused_thread_table.put(last_thread, random_index);	// Update table
	paused_thread_table.remove(thread);

	thread_id_t tid = thread->get_id();
	history->remove_waiting_write(tid);
	history->remove_waiting_thread(tid);

	model_print("thread %d is woken up\n", tid);
	threadlist[*numthreads] = tid;
	(*numthreads)++;
}

/* Wake up conditional sleeping threads if the desired write is available */
void NewFuzzer::notify_paused_thread(Thread * thread)
{
	ASSERT(paused_thread_table.contains(thread));

	int index = paused_thread_table.get(thread);
	model->getScheduler()->remove_sleep(thread);

	Thread * last_thread = paused_thread_list.back();
	paused_thread_list[index] = last_thread;
	paused_thread_list.pop_back();
	paused_thread_table.put(last_thread, index);	// Update table
	paused_thread_table.remove(thread);

	thread_id_t tid = thread->get_id();
	history->remove_waiting_write(tid);
	history->remove_waiting_thread(tid);
}

/* Find threads that may write values that the pending read action is waiting for
 * @return True if any thread is found
 */
bool NewFuzzer::find_threads(ModelAction * pending_read)
{
	ASSERT(pending_read->is_read());

	void * location = pending_read->get_location();
	thread_id_t self_id = pending_read->get_tid();
	bool finds_waiting_for = false;

	SnapVector<FuncNode *> * func_node_list = history->getWrFuncNodes(location);
	for (uint i = 0; i < func_node_list->size(); i++) {
		FuncNode * target_node = (*func_node_list)[i];
		for (uint i = 1; i < execution->get_num_threads(); i++) {
			thread_id_t tid = int_to_id(i);
			if (tid == self_id)
				continue;

			FuncNode * node = history->get_curr_func_node(tid);
			/* It is possible that thread tid is not in any FuncNode */
			if (node == NULL)
				continue;

			int distance = node->compute_distance(target_node);
			if (distance != -1) {
				history->add_waiting_thread(self_id, tid, target_node, distance);
				finds_waiting_for = true;
				model_print("thread: %d; distance from node %d to node %d: %d\n", tid, node->get_func_id(), target_node->get_func_id(), distance);
			}
		}
	}

	return finds_waiting_for;
}

bool NewFuzzer::shouldWait(const ModelAction * act)
{
	return random() & 1;
}
