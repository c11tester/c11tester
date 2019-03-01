#include "common.h"
#include "threads-model.h"
#include "action.h"
#include "pthread.h"
#include <mutex>
#include <condition_variable>
#include <assert.h>

/* global "model" object */
#include "model.h"
#include "execution.h"

int pthread_create(pthread_t *t, const pthread_attr_t * attr,
          pthread_start_t start_routine, void * arg) {
	struct pthread_params params = { start_routine, arg };

	ModelAction *act = new ModelAction(PTHREAD_CREATE, std::memory_order_seq_cst, t, (uint64_t)&params);

	/* seq_cst is just a 'don't care' parameter */
	model->switch_to_master(act);

	return 0;
}

int pthread_join(pthread_t t, void **value_ptr) {
//	Thread *th = model->get_pthread(t);
	ModelExecution *execution = model->get_execution();
	Thread *th = execution->get_pthread(t);

	model->switch_to_master(new ModelAction(PTHREAD_JOIN, std::memory_order_seq_cst, th, id_to_int(th->get_id())));

	if ( value_ptr ) {
		// store return value
		void *rtval = th->get_pthread_return();
		*value_ptr = rtval;
	} 
	return 0;
}

void pthread_exit(void *value_ptr) {
	Thread * th = thread_current();
	model->switch_to_master(new ModelAction(THREAD_FINISH, std::memory_order_seq_cst, th));
}

int pthread_mutex_init(pthread_mutex_t *p_mutex, const pthread_mutexattr_t *) {
	std::mutex *m = new std::mutex();

	ModelExecution *execution = model->get_execution();
	execution->mutex_map.put(p_mutex, m);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *p_mutex) {
	ModelExecution *execution = model->get_execution();
	std::mutex *m = execution->mutex_map.get(p_mutex);
	m->lock();
	/* error message? */
	return 0;
}
int pthread_mutex_trylock(pthread_mutex_t *p_mutex) {
	ModelExecution *execution = model->get_execution();
	std::mutex *m = execution->mutex_map.get(p_mutex);
	return m->try_lock();
}
int pthread_mutex_unlock(pthread_mutex_t *p_mutex) {	
	ModelExecution *execution = model->get_execution();
	std::mutex *m = execution->mutex_map.get(p_mutex);
	m->unlock();
	return 0;
}

int pthread_mutex_timedlock (pthread_mutex_t *__restrict p_mutex,
				const struct timespec *__restrict abstime) {
	ModelExecution *execution = model->get_execution();
	std::mutex *m = execution->mutex_map.get(p_mutex);
	m->lock();
	return 0;
}

pthread_t pthread_self() {
	Thread* th = model->get_current_thread();
	return th->get_id();
}

int pthread_key_delete(pthread_key_t) {
	model_print("key_delete is called\n");
	return 0;
}

int pthread_cond_init(pthread_cond_t *p_cond, const pthread_condattr_t *attr) {
	std::condition_variable *v = new std::condition_variable();

	ModelExecution *execution = model->get_execution();
	execution->cond_map.put(p_cond, v);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *p_cond, pthread_mutex_t *p_mutex) {
	ModelExecution *execution = model->get_execution();
	std::condition_variable *v = execution->cond_map.get(p_cond);
	std::mutex *m = execution->mutex_map.get(p_mutex);

	v->wait(*m);
	return 0;

}

int pthread_cond_timedwait(pthread_cond_t *p_cond, 
    pthread_mutex_t *p_mutex, const struct timespec *abstime) {
	ModelExecution *execution = model->get_execution();
	std::condition_variable *v = execution->cond_map.get(p_cond);
	std::mutex *m = execution->mutex_map.get(p_mutex);

	v->wait(*m);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *p_cond) {
	// notify only one blocked thread
	ModelExecution *execution = model->get_execution();
	std::condition_variable *v = execution->cond_map.get(p_cond);

	v->notify_one();
	return 0;
}
