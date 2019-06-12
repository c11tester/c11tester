/**
 * @file cyclegraph.h
 * @brief Data structure to track ordering constraints on modification order
 *
 * Used to determine whether a total order exists that satisfies the ordering
 * constraints.
 */

#ifndef __CYCLEGRAPH_H__
#define __CYCLEGRAPH_H__

#include <inttypes.h>
#include <stdio.h>

#include "hashtable.h"
#include "config.h"
#include "mymemory.h"
#include "stl-model.h"
#include "classlist.h"

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
public:
	CycleGraph();
	~CycleGraph();

	template <typename T, typename U>
	bool addEdge(const T *from, const U *to);

	template <typename T>
	void addRMWEdge(const T *from, const ModelAction *rmw);

	bool checkForCycles() const;

	template <typename T, typename U>
	bool checkReachable(const T *from, const U *to) const;

	void startChanges();
	void commitChanges();
	void rollbackChanges();
#if SUPPORT_MOD_ORDER_DUMP
	void dumpNodes(FILE *file) const;
	void dumpGraphToFile(const char *filename) const;

	void dot_print_node(FILE *file, const ModelAction *act);
	template <typename T, typename U>
	void dot_print_edge(FILE *file, const T *from, const U *to, const char *prop);
#endif
	CycleNode * getNode_noCreate(const ModelAction *act) const;

	SNAPSHOTALLOC
private:
	bool addNodeEdge(CycleNode *fromnode, CycleNode *tonode);
	void putNode(const ModelAction *act, CycleNode *node);
	CycleNode * getNode(const ModelAction *act);
	bool mergeNodes(CycleNode *node1, CycleNode *node2);

	HashTable<const CycleNode *, const CycleNode *, uintptr_t, 4, model_malloc, model_calloc, model_free> *discovered;
	ModelVector<const CycleNode *> * queue;


	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;

#if SUPPORT_MOD_ORDER_DUMP
	SnapVector<CycleNode *> nodeList;
#endif

	bool checkReachable(const CycleNode *from, const CycleNode *to) const;

	/** @brief A flag: true if this graph contains cycles */
	bool hasCycles;
	/** @brief The previous value of CycleGraph::hasCycles, for rollback */
	bool oldCycles;

	SnapVector<CycleNode *> rollbackvector;
	SnapVector<CycleNode *> rmwrollbackvector;
};

/**
 * @brief A node within a CycleGraph; corresponds either to one ModelAction
 */
class CycleNode {
public:
	CycleNode(const ModelAction *act);
	bool addEdge(CycleNode *node);
	CycleNode * getEdge(unsigned int i) const;
	unsigned int getNumEdges() const;
	CycleNode * removeEdge();

	bool setRMW(CycleNode *);
	CycleNode * getRMW() const;
	void clearRMW() { hasRMW = NULL; }
	const ModelAction * getAction() const { return action; }

	SNAPSHOTALLOC
private:
	/** @brief The ModelAction that this node represents */
	const ModelAction *action;

	/** @brief The edges leading out from this node */
	SnapVector<CycleNode *> edges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode *hasRMW;
};

#endif	/* __CYCLEGRAPH_H__ */
