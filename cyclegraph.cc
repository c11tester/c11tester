#include "cyclegraph.h"
#include "action.h"
#include "common.h"
#include "threads-model.h"
#include "clockvector.h"

/** Initializes a CycleGraph object. */
CycleGraph::CycleGraph() :
	queue(new SnapVector<const CycleNode *>())
{
}

/** CycleGraph destructor */
CycleGraph::~CycleGraph()
{
	delete queue;
}

/**
 * Add a CycleNode to the graph, corresponding to a store ModelAction
 * @param act The write action that should be added
 * @param node The CycleNode that corresponds to the store
 */
void CycleGraph::putNode(const ModelAction *act, CycleNode *node)
{
	actionToNode.put(act, node);
#if SUPPORT_MOD_ORDER_DUMP
	nodeList.push_back(node);
#endif
}

/** @return The corresponding CycleNode, if exists; otherwise NULL */
CycleNode * CycleGraph::getNode_noCreate(const ModelAction *act) const
{
	return actionToNode.get(act);
}

/**
 * @brief Returns the CycleNode corresponding to a given ModelAction
 *
 * Gets (or creates, if none exist) a CycleNode corresponding to a ModelAction
 *
 * @param action The ModelAction to find a node for
 * @return The CycleNode paired with this action
 */
CycleNode * CycleGraph::getNode(ModelAction *action)
{
	CycleNode *node = getNode_noCreate(action);
	if (node == NULL) {
		node = new CycleNode(action);
		putNode(action, node);
	}
	return node;
}

/**
 * Adds an edge between two CycleNodes.
 * @param fromnode The edge comes from this CycleNode
 * @param tonode The edge points to this CycleNode
 * @return True, if new edge(s) are added; otherwise false
 */
void CycleGraph::addNodeEdge(CycleNode *fromnode, CycleNode *tonode, bool forceedge)
{
	//quick check whether edge is redundant
	if (checkReachable(fromnode, tonode) && !forceedge) {
		return;
	}

	/*
	 * If the fromnode has a rmwnode, we should
	 * follow its RMW chain to add an edge at the end.
	 */
	while (CycleNode * nextnode = fromnode->getRMW()) {
		if (nextnode == tonode)
			break;
		fromnode = nextnode;
	}

	fromnode->addEdge(tonode);	//Add edge to edgeSrcNode

	/* Propagate clock vector changes */
	if (tonode->cv->merge(fromnode->cv)) {
		queue->push_back(tonode);
		while(!queue->empty()) {
			const CycleNode *node = queue->back();
			queue->pop_back();
			unsigned int numedges = node->getNumEdges();
			for(unsigned int i = 0;i < numedges;i++) {
				CycleNode * enode = node->getEdge(i);
				if (enode->cv->merge(node->cv))
					queue->push_back(enode);
			}
		}
	}
}

/**
 * @brief Add an edge between a write and the RMW which reads from it
 *
 * Handles special case of a RMW action, where the ModelAction rmw reads from
 * the ModelAction from. The key differences are:
 *  -# No write can occur in between the @a rmw and @a from actions.
 *  -# Only one RMW action can read from a given write.
 *
 * @param from The edge comes from this ModelAction
 * @param rmw The edge points to this ModelAction; this action must read from
 * the ModelAction from
 */
void CycleGraph::addRMWEdge(ModelAction *from, ModelAction *rmw)
{
	ASSERT(from);
	ASSERT(rmw);

	CycleNode *fromnode = getNode(from);
	CycleNode *rmwnode = getNode(rmw);
	/* We assume that this RMW has no RMW reading from it yet */
	ASSERT(!rmwnode->getRMW());

	fromnode->setRMW(rmwnode);

	/* Transfer all outgoing edges from the from node to the rmw node */
	/* This process should not add a cycle because either:
	 * (1) The rmw should not have any incoming edges yet if it is the
	 * new node or
	 * (2) the fromnode is the new node and therefore it should not
	 * have any outgoing edges.
	 */
	for (unsigned int i = 0;i < fromnode->getNumEdges();i++) {
		CycleNode *tonode = fromnode->getEdge(i);
		if (tonode != rmwnode) {
			rmwnode->addEdge(tonode);
		}
		tonode->removeInEdge(fromnode);
	}
	fromnode->edges.clear();

	addNodeEdge(fromnode, rmwnode, true);
}

void CycleGraph::addEdges(SnapList<ModelAction *> * edgeset, ModelAction *to) {
	for(sllnode<ModelAction*> * it = edgeset->begin();it!=NULL;) {
		ModelAction *act = it->getVal();
		CycleNode *node = getNode(act);
		sllnode<ModelAction*> * it2 = it;
		it2=it2->getNext();
		for(;it2!=NULL; ) {
			ModelAction *act2 = it2->getVal();
			CycleNode *node2 = getNode(act2);
			if (checkReachable(node, node2)) {
				it = edgeset->erase(it);
				goto endouterloop;
			} else if (checkReachable(node2, node)) {
				it2 = edgeset->erase(it2);
				goto endinnerloop;
			}
			it2=it2->getNext();
endinnerloop:
			;
		}
		it=it->getNext();
endouterloop:
		;
	}
	for(sllnode<ModelAction*> *it = edgeset->begin();it!=NULL;it=it->getNext()) {
		ModelAction *from = it->getVal();
		addEdge(from, to, from->get_tid() == to->get_tid());
	}
}

/**
 * @brief Adds an edge between objects
 *
 * This function will add an edge between any two objects which can be
 * associated with a CycleNode. That is, if they have a CycleGraph::getNode
 * implementation.
 *
 * The object to is ordered after the object from.
 *
 * @param to The edge points to this object, of type T
 * @param from The edge comes from this object, of type U
 * @return True, if new edge(s) are added; otherwise false
 */

void CycleGraph::addEdge(ModelAction *from, ModelAction *to)
{
	ASSERT(from);
	ASSERT(to);

	CycleNode *fromnode = getNode(from);
	CycleNode *tonode = getNode(to);

	addNodeEdge(fromnode, tonode, false);
}

void CycleGraph::addEdge(ModelAction *from, ModelAction *to, bool forceedge)
{
	ASSERT(from);
	ASSERT(to);

	CycleNode *fromnode = getNode(from);
	CycleNode *tonode = getNode(to);

	addNodeEdge(fromnode, tonode, forceedge);
}

#if SUPPORT_MOD_ORDER_DUMP

static void print_node(FILE *file, const CycleNode *node, int label)
{
	const ModelAction *act = node->getAction();
	modelclock_t idx = act->get_seq_number();
	fprintf(file, "N%u", idx);
	if (label)
		fprintf(file, " [label=\"N%u, T%u\"]", idx, act->get_tid());
}

static void print_edge(FILE *file, const CycleNode *from, const CycleNode *to, const char *prop)
{
	print_node(file, from, 0);
	fprintf(file, " -> ");
	print_node(file, to, 0);
	if (prop && strlen(prop))
		fprintf(file, " [%s]", prop);
	fprintf(file, ";\n");
}

void CycleGraph::dot_print_node(FILE *file, const ModelAction *act)
{
	print_node(file, getNode(act), 1);
}

void CycleGraph::dot_print_edge(FILE *file, const ModelAction *from, const ModelAction *to, const char *prop)
{
	CycleNode *fromnode = getNode(from);
	CycleNode *tonode = getNode(to);

	print_edge(file, fromnode, tonode, prop);
}

void CycleGraph::dumpNodes(FILE *file) const
{
	for (unsigned int i = 0;i < nodeList.size();i++) {
		CycleNode *n = nodeList[i];
		print_node(file, n, 1);
		fprintf(file, ";\n");
		if (n->getRMW())
			print_edge(file, n, n->getRMW(), "style=dotted");
		for (unsigned int j = 0;j < n->getNumEdges();j++)
			print_edge(file, n, n->getEdge(j), NULL);
	}
}

void CycleGraph::dumpGraphToFile(const char *filename) const
{
	char buffer[200];
	sprintf(buffer, "%s.dot", filename);
	FILE *file = fopen(buffer, "w");
	fprintf(file, "digraph %s {\n", filename);
	dumpNodes(file);
	fprintf(file, "}\n");
	fclose(file);
}
#endif

/**
 * Checks whether one CycleNode can reach another.
 * @param from The CycleNode from which to begin exploration
 * @param to The CycleNode to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
bool CycleGraph::checkReachable(const CycleNode *from, const CycleNode *to) const
{
	return to->cv->synchronized_since(from->action);
}

/**
 * Checks whether one ModelAction can reach another ModelAction
 * @param from The ModelAction from which to begin exploration
 * @param to The ModelAction to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
bool CycleGraph::checkReachable(const ModelAction *from, const ModelAction *to) const
{
	CycleNode *fromnode = getNode_noCreate(from);
	CycleNode *tonode = getNode_noCreate(to);

	if (!fromnode || !tonode)
		return false;

	return checkReachable(fromnode, tonode);
}

void CycleGraph::freeAction(const ModelAction * act) {
	CycleNode *cn = actionToNode.remove(act);
	for(unsigned int i=0;i<cn->edges.size();i++) {
		CycleNode *dst = cn->edges[i];
		dst->removeInEdge(cn);
	}
	for(unsigned int i=0;i<cn->inedges.size();i++) {
		CycleNode *src = cn->inedges[i];
		src->removeEdge(cn);
	}
	delete cn;
}

/**
 * @brief Constructor for a CycleNode
 * @param act The ModelAction for this node
 */
CycleNode::CycleNode(ModelAction *act) :
	action(act),
	hasRMW(NULL),
	cv(new ClockVector(NULL, act))
{
}

CycleNode::~CycleNode() {
	delete cv;
}

void CycleNode::removeInEdge(CycleNode *src) {
	for(unsigned int i=0;i < inedges.size();i++) {
		if (inedges[i] == src) {
			inedges[i] = inedges[inedges.size()-1];
			inedges.pop_back();
			break;
		}
	}
}

void CycleNode::removeEdge(CycleNode *dst) {
	for(unsigned int i=0;i < edges.size();i++) {
		if (edges[i] == dst) {
			edges[i] = edges[edges.size()-1];
			edges.pop_back();
			break;
		}
	}
}

/**
 * @param i The index of the edge to return
 * @returns The CycleNode edge indexed by i
 */
CycleNode * CycleNode::getEdge(unsigned int i) const
{
	return edges[i];
}

/** @returns The number of edges leaving this CycleNode */
unsigned int CycleNode::getNumEdges() const
{
	return edges.size();
}

/**
 * @param i The index of the edge to return
 * @returns The CycleNode edge indexed by i
 */
CycleNode * CycleNode::getInEdge(unsigned int i) const
{
	return inedges[i];
}

/** @returns The number of edges leaving this CycleNode */
unsigned int CycleNode::getNumInEdges() const
{
	return inedges.size();
}

/**
 * Adds an edge from this CycleNode to another CycleNode.
 * @param node The node to which we add a directed edge
 * @return True if this edge is a new edge; false otherwise
 */
void CycleNode::addEdge(CycleNode *node)
{
	for (unsigned int i = 0;i < edges.size();i++)
		if (edges[i] == node)
			return;
	edges.push_back(node);
	node->inedges.push_back(this);
}

/** @returns the RMW CycleNode that reads from the current CycleNode */
CycleNode * CycleNode::getRMW() const
{
	return hasRMW;
}

/**
 * Set a RMW action node that reads from the current CycleNode.
 * @param node The RMW that reads from the current node
 * @return True, if this node already was read by another RMW; false otherwise
 * @see CycleGraph::addRMWEdge
 */
bool CycleNode::setRMW(CycleNode *node)
{
	if (hasRMW != NULL)
		return true;
	hasRMW = node;
	return false;
}
