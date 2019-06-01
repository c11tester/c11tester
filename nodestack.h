/** @file nodestack.h
 *  @brief Stack of operations for use in backtracking.
*/

#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "schedule.h"
#include "stl-model.h"

class ModelAction;
class Thread;

struct fairness_info {
	unsigned int enabled_count;
	unsigned int turns;
	bool priority;
};


/**
 * @brief A single node in a NodeStack
 *
 * Represents a single node in the NodeStack. Each Node is associated with up
 * to one action and up to one parent node. A node holds information
 * regarding the last action performed (the "associated action"), the thread
 * choices that have been explored (explored_children) and should be explored
 * (backtrack), and the actions that the last action may read from.
 */
class Node {
public:
	Node(const struct model_params *params, ModelAction *act, Node *par,
			int nthreads);
	~Node();

	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;
	enabled_type_t enabled_status(thread_id_t tid) const;

	ModelAction * get_action() const { return action; }
	void set_uninit_action(ModelAction *act) { uninit_action = act; }
	ModelAction * get_uninit_action() const { return uninit_action; }

	bool has_priority(thread_id_t tid) const;
	void update_yield(Scheduler *);
	bool has_priority_over(thread_id_t tid, thread_id_t tid2) const;
	int get_num_threads() const { return num_threads; }
	/** @return the parent Node to this Node; that is, the action that
	 * occurred previously in the stack. */
	Node * get_parent() const { return parent; }



	void print() const;

	MEMALLOC
private:
	const struct model_params * get_params() const { return params; }
	ModelAction * const action;
	const struct model_params * const params;

	/** @brief ATOMIC_UNINIT action which was created at this Node */
	ModelAction *uninit_action;
	Node * const parent;
	const int num_threads;
};

typedef ModelVector<Node *> node_list_t;

/**
 * @brief A stack of nodes
 *
 * Holds a Node linked-list that can be used for holding backtracking,
 * may-read-from, and replay information. It is used primarily as a
 * stack-like structure, in that backtracking points and replay nodes are
 * only removed from the top (most recent).
 */
class NodeStack {
public:
	NodeStack();
	~NodeStack();

	void register_engine(const ModelExecution *exec);

	ModelAction * explore_action(ModelAction *act, enabled_type_t * is_enabled);
	Node * get_head() const;
	Node * get_next() const;
	void reset_execution();
	void full_reset();
	int get_total_nodes() { return total_nodes; }

	void print() const;

	MEMALLOC
private:
	node_list_t node_list;

	const struct model_params * get_params() const;

	/** @brief The model-checker execution object */
	const ModelExecution *execution;

	/**
	 * @brief the index position of the current head Node
	 *
	 * This index is relative to node_list. The index should point to the
	 * current head Node. It is negative when the list is empty.
	 */
	int head_idx;

	int total_nodes;
};

#endif /* __NODESTACK_H__ */
