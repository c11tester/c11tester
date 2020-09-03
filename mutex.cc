#include "mutex.h"

#include "model.h"
#include "execution.h"
#include "threads-model.h"
#include "clockvector.h"
#include "action.h"

namespace cdsc {

mutex::mutex(int type)
{
	state.locked = NULL;
	thread_id_t tid = thread_current_id();
	state.alloc_tid = tid;
	ClockVector *cv = model->get_execution()->get_cv(tid);
	state.alloc_clock = cv  == NULL ? 0 : cv->getClock(tid);

	// For recursive mutex
	state.type = type;
	state.lock_count = 0;
}

void mutex::lock()
{
	model->switch_thread(new ModelAction(ATOMIC_LOCK, std::memory_order_seq_cst, this));
}

bool mutex::try_lock()
{
	return model->switch_thread(new ModelAction(ATOMIC_TRYLOCK, std::memory_order_seq_cst, this));
}

void mutex::unlock()
{
	model->switch_thread(new ModelAction(ATOMIC_UNLOCK, std::memory_order_seq_cst, this));
}

}
