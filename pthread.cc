#include "common.h"
#include "threads-model.h"
#include "action.h"
#include <pthread.h>
#include <mutex>

/* global "model" object */
#include "model.h"

unsigned int counter = 0;	// counter does not to be reset to zero. It is 
				// find as long as it is unique.

int pthread_create(pthread_t *t, const pthread_attr_t * attr,
          pthread_start_t start_routine, void * arg) {
	struct pthread_params params = { start_routine, arg };

	*t = counter;
	counter++;

	ModelAction *act = new ModelAction(PTHREAD_CREATE, std::memory_order_seq_cst, t, (uint64_t)&params);
	model->pthread_map[*t] = act;

	/* seq_cst is just a 'don't care' parameter */
	model->switch_to_master(act);

	return 0;
}

int pthread_join(pthread_t t, void **value_ptr) {
	ModelAction *act = model->pthread_map[t];
	Thread *th = act->get_thread_operand();

	model->switch_to_master(new ModelAction(PTHREAD_JOIN, std::memory_order_seq_cst, th, id_to_int(th->get_id())));

	// store return value
	void *rtval = th->get_pthread_return();
	*value_ptr = rtval;

	return 0;
}

void pthread_exit(void *value_ptr) {
	Thread * th = thread_current();
	model->switch_to_master(new ModelAction(THREAD_FINISH, std::memory_order_seq_cst, th));
}

int pthread_mutex_init(pthread_mutex_t *p_mutex, const pthread_mutexattr_t *) {
	if (model->mutex_map.find(p_mutex) != model->mutex_map.end() ) {
		model_print("Reinitialize a lock\n");
		// return 1;	// 0 means success; 1 means failure
	}

	std::mutex *m = new std::mutex();
	m->initialize();
	model->mutex_map[p_mutex] = m;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *p_mutex) {
	std::mutex *m = model->mutex_map[p_mutex];
	m->lock();
	/* error message? */
	return 0;
}
int pthread_mutex_trylock(pthread_mutex_t *p_mutex) {
	std::mutex *m = model->mutex_map[p_mutex];
	return m->try_lock();
	/* error message?  */
}
int pthread_mutex_unlock(pthread_mutex_t *p_mutex) {	
	std::mutex *m = model->mutex_map[p_mutex];
        m->unlock();
	return 0;
}

void check() {
	for (std::map<pthread_t, ModelAction*>::iterator it = model->pthread_map.begin(); it != model->pthread_map.end(); it++) {
		model_print("id: %d\n", it->first);
	}
}
