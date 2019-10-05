#ifndef __WAITOBJ_H__
#define __WAITOBJ_H__

#include "classlist.h"
#include "modeltypes.h"

class WaitObj {
public:
	WaitObj(thread_id_t);
	~WaitObj() {}

	thread_id_t get_tid() { return tid; }

	thrd_id_set_t * getWaitingFor() { return &waiting_for; }
	thrd_id_set_t * getWaitingBy() { return &waited_by; }
	int lookup_dist(thread_id_t other_tid);

	void print_waiting_for();
	void print_waited_by();

	SNAPSHOTALLOC
private:
	thread_id_t tid;

	/* The set of threads this thread (tid) is waiting for */
	thrd_id_set_t waiting_for;

	/* The set of threads waiting for this thread */
	thrd_id_set_t waited_by;

	HashTable<thread_id_t, int, int, 0> dist_table;
};

#endif
