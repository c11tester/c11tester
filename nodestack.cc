#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <cstdlib>

#include <string.h>

#include "nodestack.h"
#include "action.h"
#include "common.h"
#include "threads-model.h"
#include "modeltypes.h"
#include "execution.h"
#include "params.h"

/**
 * @brief Node constructor
 *
 * Constructs a single Node for use in a NodeStack. Each Node is associated
 * with exactly one ModelAction (exception: the first Node should be created
 * as an empty stub, to represent the first thread "choice") and up to one
 * parent.
 *
 * @param params The model-checker parameters
 * @param act The ModelAction to associate with this Node. May be NULL.
 * @param par The parent Node in the NodeStack. May be NULL if there is no
 * parent.
 * @param nthreads The number of threads which exist at this point in the
 * execution trace.
 */
Node::Node(const struct model_params *params, ModelAction *act, Node *par,
		int nthreads, Node *prevfairness) :
	read_from_status(READ_FROM_PAST),
	action(act),
	params(params),
	uninit_action(NULL),
	parent(par),
	num_threads(nthreads),
	explored_children(num_threads),
	backtrack(num_threads),
	fairness(num_threads),
	numBacktracks(0),
	enabled_array(NULL),
	read_from_past(),
	read_from_past_idx(0),
	misc_index(0),
	misc_max(0),
	yield_data(NULL)
{
	ASSERT(act);
	act->set_node(this);
	int currtid = id_to_int(act->get_tid());
	int prevtid = prevfairness ? id_to_int(prevfairness->action->get_tid()) : 0;

	if (get_params()->fairwindow != 0) {
		for (int i = 0; i < num_threads; i++) {
			ASSERT(i < ((int)fairness.size()));
			struct fairness_info *fi = &fairness[i];
			struct fairness_info *prevfi = (parent && i < parent->get_num_threads()) ? &parent->fairness[i] : NULL;
			if (prevfi) {
				*fi = *prevfi;
			}
			if (parent && parent->is_enabled(int_to_id(i))) {
				fi->enabled_count++;
			}
			if (i == currtid) {
				fi->turns++;
				fi->priority = false;
			}
			/* Do window processing */
			if (prevfairness != NULL) {
				if (prevfairness->parent->is_enabled(int_to_id(i)))
					fi->enabled_count--;
				if (i == prevtid) {
					fi->turns--;
				}
				/* Need full window to start evaluating
				 * conditions
				 * If we meet the enabled count and have no
				 * turns, give us priority */
				if ((fi->enabled_count >= get_params()->enabledcount) &&
						(fi->turns == 0))
					fi->priority = true;
			}
		}
	}
}

int Node::get_yield_data(int tid1, int tid2) const {
	if (tid1<num_threads && tid2 < num_threads)
		return yield_data[YIELD_INDEX(tid1,tid2,num_threads)];
	else
		return YIELD_S | YIELD_D;
}

void Node::update_yield(Scheduler * scheduler) {
	if (yield_data==NULL)
		yield_data=(int *) model_calloc(1, sizeof(int)*num_threads*num_threads);
	//handle base case
	if (parent == NULL) {
		for(int i = 0; i < num_threads*num_threads; i++) {
			yield_data[i] = YIELD_S | YIELD_D;
		}
		return;
	}
	int curr_tid=id_to_int(action->get_tid());

	for(int u = 0; u < num_threads; u++) {
		for(int v = 0; v < num_threads; v++) {
			int yield_state=parent->get_yield_data(u, v);
			bool next_enabled=scheduler->is_enabled(int_to_id(v));
			bool curr_enabled=parent->is_enabled(int_to_id(v));
			if (!next_enabled) {
				//Compute intersection of ES and E
				yield_state&=~YIELD_E;
				//Check to see if we disabled the thread
				if (u==curr_tid && curr_enabled)
					yield_state|=YIELD_D;
			}
			yield_data[YIELD_INDEX(u, v, num_threads)]=yield_state;
		}
		yield_data[YIELD_INDEX(u, curr_tid, num_threads)]=(yield_data[YIELD_INDEX(u, curr_tid, num_threads)]&~YIELD_P)|YIELD_S;
	}
	//handle curr.yield(t) part of computation
	if (action->is_yield()) {
		for(int v = 0; v < num_threads; v++) {
			int yield_state=yield_data[YIELD_INDEX(curr_tid, v, num_threads)];
			if ((yield_state & (YIELD_E | YIELD_D)) && (!(yield_state & YIELD_S)))
				yield_state |= YIELD_P;
			yield_state &= YIELD_P;
			if (scheduler->is_enabled(int_to_id(v))) {
				yield_state|=YIELD_E;
			}
			yield_data[YIELD_INDEX(curr_tid, v, num_threads)]=yield_state;
		}
	}
}

/** @brief Node desctructor */
Node::~Node()
{
	delete action;
	if (uninit_action)
		delete uninit_action;
	if (enabled_array)
		model_free(enabled_array);
	if (yield_data)
		model_free(yield_data);
}

/** Prints debugging info for the ModelAction associated with this Node */
void Node::print() const
{
	action->print();
	model_print("          thread status: ");
	if (enabled_array) {
		for (int i = 0; i < num_threads; i++) {
			char str[20];
			enabled_type_to_string(enabled_array[i], str);
			model_print("[%d: %s]", i, str);
		}
		model_print("\n");
	} else
		model_print("(info not available)\n");
	model_print("          backtrack: %s", backtrack_empty() ? "empty" : "non-empty ");
	for (int i = 0; i < (int)backtrack.size(); i++)
		if (backtrack[i] == true)
			model_print("[%d]", i);
	model_print("\n");

	model_print("          read from past: %s", read_from_past_empty() ? "empty" : "non-empty ");
	for (int i = read_from_past_idx + 1; i < (int)read_from_past.size(); i++)
		model_print("[%d]", read_from_past[i]->get_seq_number());
	model_print("\n");

	model_print("          misc: %s\n", misc_empty() ? "empty" : "non-empty");
}

/****************************** threads backtracking **************************/

/**
 * Checks if the Thread associated with this thread ID has been explored from
 * this Node already.
 * @param tid is the thread ID to check
 * @return true if this thread choice has been explored already, false
 * otherwise
 */
bool Node::has_been_explored(thread_id_t tid) const
{
	int id = id_to_int(tid);
	return explored_children[id];
}

/**
 * Checks if the backtracking set is empty.
 * @return true if the backtracking set is empty
 */
bool Node::backtrack_empty() const
{
	return (numBacktracks == 0);
}

void Node::explore(thread_id_t tid)
{
	int i = id_to_int(tid);
	ASSERT(i < ((int)backtrack.size()));
	if (backtrack[i]) {
		backtrack[i] = false;
		numBacktracks--;
	}
	explored_children[i] = true;
}

/**
 * Mark the appropriate backtracking information for exploring a thread choice.
 * @param act The ModelAction to explore
 */
void Node::explore_child(ModelAction *act, enabled_type_t *is_enabled)
{
	if (!enabled_array)
		enabled_array = (enabled_type_t *)model_malloc(sizeof(enabled_type_t) * num_threads);
	if (is_enabled != NULL)
		memcpy(enabled_array, is_enabled, sizeof(enabled_type_t) * num_threads);
	else {
		for (int i = 0; i < num_threads; i++)
			enabled_array[i] = THREAD_DISABLED;
	}

	explore(act->get_tid());
}

/**
 * Records a backtracking reference for a thread choice within this Node.
 * Provides feedback as to whether this thread choice is already set for
 * backtracking.
 * @return false if the thread was already set to be backtracked, true
 * otherwise
 */
bool Node::set_backtrack(thread_id_t id)
{
	int i = id_to_int(id);
	ASSERT(i < ((int)backtrack.size()));
	if (backtrack[i])
		return false;
	backtrack[i] = true;
	numBacktracks++;
	return true;
}

thread_id_t Node::get_next_backtrack()
{
	/** @todo Find next backtrack */
	unsigned int i;
	for (i = 0; i < backtrack.size(); i++)
		if (backtrack[i] == true)
			break;
	/* Backtrack set was empty? */
	ASSERT(i != backtrack.size());

	backtrack[i] = false;
	numBacktracks--;
	return int_to_id(i);
}

void Node::clear_backtracking()
{
	for (unsigned int i = 0; i < backtrack.size(); i++)
		backtrack[i] = false;
	for (unsigned int i = 0; i < explored_children.size(); i++)
		explored_children[i] = false;
	numBacktracks = 0;
}

/************************** end threads backtracking **************************/

void Node::set_misc_max(int i)
{
	misc_max = i;
}

int Node::get_misc() const
{
	return misc_index;
}

bool Node::increment_misc()
{
	return (misc_index < misc_max) && ((++misc_index) < misc_max);
}

bool Node::misc_empty() const
{
	return (misc_index + 1) >= misc_max;
}

bool Node::is_enabled(Thread *t) const
{
	int thread_id = id_to_int(t->get_id());
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

enabled_type_t Node::enabled_status(thread_id_t tid) const
{
	int thread_id = id_to_int(tid);
	if (thread_id < num_threads)
		return enabled_array[thread_id];
	else
		return THREAD_DISABLED;
}

bool Node::is_enabled(thread_id_t tid) const
{
	int thread_id = id_to_int(tid);
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

bool Node::has_priority(thread_id_t tid) const
{
	return fairness[id_to_int(tid)].priority;
}

bool Node::has_priority_over(thread_id_t tid1, thread_id_t tid2) const
{
	return get_yield_data(id_to_int(tid1), id_to_int(tid2)) & YIELD_P;
}

/*********************************** read from ********************************/

/**
 * Get the current state of the may-read-from set iteration
 * @return The read-from type we should currently be checking (past)
 */
read_from_type_t Node::get_read_from_status()
{
	if (read_from_status == READ_FROM_PAST && read_from_past.empty())
		increment_read_from();
	return read_from_status;
}

/**
 * Iterate one step in the may-read-from iteration. This includes a step in
 * reading from the either the past or the future.
 * @return True if there is a new read-from to explore; false otherwise
 */
bool Node::increment_read_from()
{
	if (increment_read_from_past()) {
	       read_from_status = READ_FROM_PAST;
	       return true;
	}
	read_from_status = READ_FROM_NONE;
	return false;
}

/**
 * @return True if there are any new read-froms to explore
 */
bool Node::read_from_empty() const
{
  return read_from_past_empty();
}

/**
 * Get the total size of the may-read-from set, including both past
 * @return The size of may-read-from
 */
unsigned int Node::read_from_size() const
{
  return read_from_past.size();
}

/******************************* end read from ********************************/

/****************************** read from past ********************************/

/** @brief Prints info about read_from_past set */
void Node::print_read_from_past()
{
	for (unsigned int i = 0; i < read_from_past.size(); i++)
		read_from_past[i]->print();
}

/**
 * Add an action to the read_from_past set.
 * @param act is the action to add
 */
void Node::add_read_from_past(const ModelAction *act)
{
	read_from_past.push_back(act);
}

/**
 * Gets the next 'read_from_past' action from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The first element in read_from_past
 */
const ModelAction * Node::get_read_from_past() const
{
	if (read_from_past_idx < read_from_past.size()) {
		int random_index = rand() % read_from_past.size(); 
		return read_from_past[random_index];
	}
//		return read_from_past[read_from_past_idx];
	else
		return NULL;
}

const ModelAction * Node::get_read_from_past(int i) const
{
	return read_from_past[i];
}

int Node::get_read_from_past_size() const
{
	return read_from_past.size();
}

/**
 * Checks whether the readsfrom set for this node is empty.
 * @return true if the readsfrom set is empty.
 */
bool Node::read_from_past_empty() const
{
	return ((read_from_past_idx + 1) >= read_from_past.size());
}

/**
 * Increments the index into the readsfrom set to explore the next item.
 * @return Returns false if we have explored all items.
 */
bool Node::increment_read_from_past()
{
	DBG();
	if (read_from_past_idx < read_from_past.size()) {
		read_from_past_idx++;
		return read_from_past_idx < read_from_past.size();
	}
	return false;
}

/************************** end read from past ********************************/


/**
 * Increments some behavior's index, if a new behavior is available
 * @return True if there is a new behavior available; otherwise false
 */
bool Node::increment_behaviors()
{
	/* satisfy a different misc_index values */
	if (increment_misc())
		return true;
	/* read from a different value */
	if (increment_read_from())
		return true;
	return false;
}

NodeStack::NodeStack() :
	node_list(),
	head_idx(-1),
	total_nodes(0)
{
	total_nodes++;
}

NodeStack::~NodeStack()
{
	for (unsigned int i = 0; i < node_list.size(); i++)
		delete node_list[i];
}

/**
 * @brief Register the model-checker object with this NodeStack
 * @param exec The execution structure for the ModelChecker
 */
void NodeStack::register_engine(const ModelExecution *exec)
{
	this->execution = exec;
}

const struct model_params * NodeStack::get_params() const
{
	return execution->get_params();
}

void NodeStack::print() const
{
	model_print("............................................\n");
	model_print("NodeStack printing node_list:\n");
	for (unsigned int it = 0; it < node_list.size(); it++) {
		if ((int)it == this->head_idx)
			model_print("vvv following action is the current iterator vvv\n");
		node_list[it]->print();
	}
	model_print("............................................\n");
}

/** Note: The is_enabled set contains what actions were enabled when
 *  act was chosen. */
ModelAction * NodeStack::explore_action(ModelAction *act, enabled_type_t *is_enabled)
{
	DBG();

	if ((head_idx + 1) < (int)node_list.size()) {
		head_idx++;
		return node_list[head_idx]->get_action();
	}

	/* Record action */
	Node *head = get_head();
	Node *prevfairness = NULL;
	if (head) {
		head->explore_child(act, is_enabled);
		if (get_params()->fairwindow != 0 && head_idx > (int)get_params()->fairwindow)
			prevfairness = node_list[head_idx - get_params()->fairwindow];
	}

	int next_threads = execution->get_num_threads();
	if (act->get_type() == THREAD_CREATE || act->get_type() == PTHREAD_CREATE ) // may need to be changed
		next_threads++;
	node_list.push_back(new Node(get_params(), act, head, next_threads, prevfairness));
	total_nodes++;
	head_idx++;
	return NULL;
}

/**
 * Empties the stack of all trailing nodes after a given position and calls the
 * destructor for each. This function is provided an offset which determines
 * how many nodes (relative to the current replay state) to save before popping
 * the stack.
 * @param numAhead gives the number of Nodes (including this Node) to skip over
 * before removing nodes.
 */
void NodeStack::pop_restofstack(int numAhead)
{
	/* Diverging from previous execution; clear out remainder of list */
	unsigned int it = head_idx + numAhead;
	for (unsigned int i = it; i < node_list.size(); i++)
		delete node_list[i];
	node_list.resize(it);
	node_list.back()->clear_backtracking();
}

/** Reset the node stack. */
void NodeStack::full_reset() 
{
	for (unsigned int i = 0; i < node_list.size(); i++)
		delete node_list[i];
	node_list.clear();
	reset_execution();
	total_nodes = 1;
}

Node * NodeStack::get_head() const
{
	if (node_list.empty() || head_idx < 0)
		return NULL;
	return node_list[head_idx];
}

Node * NodeStack::get_next() const
{
	if (node_list.empty()) {
		DEBUG("Empty\n");
		return NULL;
	}
	unsigned int it = head_idx + 1;
	if (it == node_list.size()) {
		DEBUG("At end\n");
		return NULL;
	}
	return node_list[it];
}

void NodeStack::reset_execution()
{
	head_idx = -1;
}
