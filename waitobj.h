#ifndef __WAITOBJ_H__
#define __WAITOBJ_H__

#include "classlist.h"
#include "modeltypes.h"

typedef HashTable<FuncNode *, int, uintptr_t, 0> dist_map_t;
typedef HashSet<FuncNode *, uintptr_t, 0> node_set_t;
typedef HSIterator<FuncNode *, uintptr_t, 0> node_set_iter;

class WaitObj {
public:
	WaitObj(thread_id_t);
	~WaitObj();

	thread_id_t get_tid() { return tid; }

	void add_waiting_for(thread_id_t other, FuncNode * node, int dist);
	void add_waited_by(thread_id_t other);
	bool remove_waiting_for_node(thread_id_t other, FuncNode * node);
	void remove_waiting_for(thread_id_t other);
	void remove_waited_by(thread_id_t other);

	thrd_id_set_t * getWaitingFor() { return &waiting_for; }
	thrd_id_set_t * getWaitedBy() { return &waited_by; }

	node_set_t * getTargetNodes(thread_id_t tid);
	int lookup_dist(thread_id_t tid, FuncNode * target);

	bool incr_counter(thread_id_t tid);

	void clear_waiting_for();

	void print_waiting_for(bool verbose = false);
	void print_waited_by();

	SNAPSHOTALLOC
private:
	thread_id_t tid;

	/* The set of threads this thread (tid) is waiting for */
	thrd_id_set_t waiting_for;

	/* The set of threads waiting for this thread */
	thrd_id_set_t waited_by;

	SnapVector<dist_map_t *> thrd_dist_maps;
	SnapVector<node_set_t *> thrd_target_nodes;

	/* Count the number of actions for threads that
	 * this thread is waiting for */
	SnapVector<uint32_t> thrd_action_counters;

	dist_map_t * getDistMap(thread_id_t tid);
};

#endif
