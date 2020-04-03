/**
 * @file mutex
 * @brief C++11 mutex interface header
 */

#ifndef __CXX_MUTEX__
#define __CXX_MUTEX__

#include "modeltypes.h"
#include "mymemory.h"
#include "mypthread.h"

namespace cdsc {
struct mutex_state {
	void *locked;	/* Thread holding the lock */
	thread_id_t alloc_tid;
	modelclock_t alloc_clock;
	int type;
	int lock_count;
};

class mutex {
public:
	mutex(int type = PTHREAD_MUTEX_DEFAULT);
	~mutex() {}
	void lock();
	bool try_lock();
	void unlock();
	struct mutex_state * get_state() {return &state;}

private:
	struct mutex_state state;
};

class snapmutex : public mutex {
public:
	snapmutex(int type = 0) : mutex(type)
	{ }
	SNAPSHOTALLOC
};
}
#endif	/* __CXX_MUTEX__ */
