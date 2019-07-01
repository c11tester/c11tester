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
#include "classlist.h"

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
	Node(ModelAction *act);
	~Node();

	ModelAction * get_action() const { return action; }
	void set_uninit_action(ModelAction *act) { uninit_action = act; }
	ModelAction * get_uninit_action() const { return uninit_action; }
	void print() const;

	SNAPSHOTALLOC
private:
	ModelAction * const action;

	/** @brief ATOMIC_UNINIT action which was created at this Node */
	ModelAction *uninit_action;
};

typedef SnapVector<Node *> node_list_t;

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
	ModelAction * explore_action(ModelAction *act);
	Node * get_head() const;
	Node * get_next() const;
	void reset_execution();
	void full_reset();
	void print() const;

	SNAPSHOTALLOC
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
};

#endif	/* __NODESTACK_H__ */
