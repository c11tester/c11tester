/**
 * @file mutex
 * @brief C++11 mutex interface header
 */

#ifndef __CXX_MUTEX__
#define __CXX_MUTEX__

#include "modeltypes.h"

namespace cdsc {
	struct mutex_state {
		void *locked; /* Thread holding the lock */
		thread_id_t alloc_tid;
		modelclock_t alloc_clock;
		int init; // WL
	};

	class mutex {
	public:
		mutex();
		~mutex() {}
		void lock();
		bool try_lock();
		void unlock();
		struct mutex_state * get_state() {return &state;}
		void initialize() { state.init = 1; } // WL
		bool is_initialized() { return state.init == 1; }
		
	private:
		struct mutex_state state;
	};
}
#endif /* __CXX_MUTEX__ */
