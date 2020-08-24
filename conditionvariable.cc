#include "mutex.h"
#include "model.h"
#include <condition_variable>
#include "action.h"

namespace cdsc {

condition_variable::condition_variable() {

}

condition_variable::~condition_variable() {

}

void condition_variable::notify_one() {
	model->switch_thread(new ModelAction(ATOMIC_NOTIFY_ONE, std::memory_order_seq_cst, this));
}

void condition_variable::notify_all() {
	model->switch_thread(new ModelAction(ATOMIC_NOTIFY_ALL, std::memory_order_seq_cst, this));
}

void condition_variable::wait(mutex& lock) {
	model->switch_thread(new ModelAction(ATOMIC_WAIT, std::memory_order_seq_cst, this, (uint64_t) &lock));
	//relock as a second action
	lock.lock();
}
}

