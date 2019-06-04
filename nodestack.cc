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
 * @param act The ModelAction to associate with this Node. May be NULL.
 * @param par The parent Node in the NodeStack. May be NULL if there is no
 * parent.
 * @param nthreads The number of threads which exist at this point in the
 * execution trace.
 */
Node::Node(ModelAction *act, Node *par, int nthreads) :
	action(act),
	uninit_action(NULL),
	parent(par),
	num_threads(nthreads)
{
	ASSERT(act);
	act->set_node(this);
}

/** @brief Node desctructor */
Node::~Node()
{
	delete action;
	if (uninit_action)
		delete uninit_action;
}

/** Prints debugging info for the ModelAction associated with this Node */
void Node::print() const
{
	action->print();
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
ModelAction * NodeStack::explore_action(ModelAction *act)
{
	DBG();

	/* Record action */
	Node *head = get_head();

	int next_threads = execution->get_num_threads();
	if (act->get_type() == THREAD_CREATE || act->get_type() == PTHREAD_CREATE ) // may need to be changed
		next_threads++;
	node_list.push_back(new Node(act, head, next_threads));
	total_nodes++;
	head_idx++;
	return NULL;
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
