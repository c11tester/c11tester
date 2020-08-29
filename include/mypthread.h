/**
 * @file pthread.h
 * @brief C11 pthread.h interface header
 */
#ifndef PTHREAD_H
#define PTHREAD_H

#include <threads.h>
#include <sched.h>
#include <pthread.h>

/* pthread mutex types
   enum
   {
   PTHREAD_MUTEX_NORMAL
   PTHREAD_MUTEX_RECURSIVE
   PTHREAD_MUTEX_ERRORCHECK
   PTHREAD_MUTEX_DEFAULT
   };*/

typedef void *(*pthread_start_t)(void *);

struct pthread_params {
	pthread_start_t func;
	void *arg;
};

struct pthread_attr
{
	/* Scheduler parameters and priority.  */
	struct sched_param schedparam;
	int schedpolicy;
	/* Various flags like detachstate, scope, etc.  */
	int flags;
	/* Size of guard area.  */
	size_t guardsize;
	/* Stack handling.  */
	void *stackaddr;
	size_t stacksize;
	/* Affinity map.  */
	cpu_set_t *cpuset;
	size_t cpusetsize;
};

extern "C" {
int user_main(int, char**);
}

#endif
