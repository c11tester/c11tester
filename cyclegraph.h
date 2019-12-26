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
	void addEdges(SnapList<ModelAction *> * edgeset, ModelAction *to);
	void addEdge(ModelAction *from, ModelAction *to);
	void addEdge(ModelAction *from, ModelAction *to, bool forceedge);
	void addRMWEdge(ModelAction *from, ModelAction *rmw);
	bool checkReachable(const ModelAction *from, const ModelAction *to) const;
	void freeAction(const ModelAction * act);
#if SUPPORT_MOD_ORDER_DUMP
	void dumpNodes(FILE *file) const;
	void dumpGraphToFile(const char *filename) const;
	void dot_print_node(FILE *file, const ModelAction *act);
	void dot_print_edge(FILE *file, const ModelAction *from, const ModelAction *to, const char *prop);
#endif

	CycleNode * getNode_noCreate(const ModelAction *act) const;
	SNAPSHOTALLOC
private:
	void addNodeEdge(CycleNode *fromnode, CycleNode *tonode, bool forceedge);
	void putNode(const ModelAction *act, CycleNode *node);
	CycleNode * getNode(ModelAction *act);

	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
	SnapVector<const CycleNode *> * queue;

#if SUPPORT_MOD_ORDER_DUMP
	SnapVector<CycleNode *> nodeList;
#endif

	bool checkReachable(const CycleNode *from, const CycleNode *to) const;
};

/**
 * @brief A node within a CycleGraph; corresponds either to one ModelAction
 */
class CycleNode {
public:
	CycleNode(ModelAction *act);
	void addEdge(CycleNode *node);
	CycleNode * getEdge(unsigned int i) const;
	unsigned int getNumEdges() const;
	CycleNode * getInEdge(unsigned int i) const;
	unsigned int getNumInEdges() const;
	bool setRMW(CycleNode *);
	CycleNode * getRMW() const;
	void clearRMW() { hasRMW = NULL; }
	ModelAction * getAction() const { return action; }
	void removeInEdge(CycleNode *src);
	void removeEdge(CycleNode *dst);
	~CycleNode();

	SNAPSHOTALLOC
private:
	/** @brief The ModelAction that this node represents */
	ModelAction *action;

	/** @brief The edges leading out from this node */
	SnapVector<CycleNode *> edges;

	/** @brief The edges leading in from this node */
	SnapVector<CycleNode *> inedges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode *hasRMW;

	/** ClockVector for this Node. */
	ClockVector *cv;
	friend class CycleGraph;
};

#endif	/* __CYCLEGRAPH_H__ */
