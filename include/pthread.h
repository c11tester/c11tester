/**
 * @file pthread.h
 * @brief C11 pthread.h interface header
 */
#ifndef PTHREAD_H
#define PTHREAD_H 1

#include <threads.h>
#include <sched.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>

/* Mutex types.  */
enum
{
  PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_ADAPTIVE_NP
#if defined __USE_UNIX98 || defined __USE_XOPEN2K8
  ,
  PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
#endif
#ifdef __USE_GNU
  /* For compatibility.  */ 
  , PTHREAD_MUTEX_FAST_NP = PTHREAD_MUTEX_TIMED_NP
#endif
};

typedef void *(*pthread_start_t)(void *);

struct pthread_params {
    pthread_start_t func;
    void *arg;
};

extern "C" {
int pthread_create(pthread_t *, const pthread_attr_t *,
          void *(*start_routine) (void *), void * arg);
void pthread_exit(void *);
int pthread_join(pthread_t, void **);

pthread_t pthread_self(void);

int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutex_timedlock (pthread_mutex_t *__restrict p_mutex,
				const struct timespec *__restrict __abstime);

int pthread_cond_init(pthread_cond_t *p_cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *p_cond, pthread_mutex_t *p_mutex);
int pthread_cond_timedwait(pthread_cond_t *p_cond, 
    pthread_mutex_t *p_mutex, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *);

void pthread_cleanup_push(void (*routine)(void*), void *arg );

int user_main(int, char**);

// --- not implemented yet ---

int pthread_attr_destroy(pthread_attr_t *);
int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
int pthread_attr_getguardsize(const pthread_attr_t *, size_t *);
int pthread_attr_getinheritsched(const pthread_attr_t *, int *);
int pthread_attr_getschedparam(const pthread_attr_t *,
          struct sched_param *);
int pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
int pthread_attr_getscope(const pthread_attr_t *, int *);
int pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
int pthread_attr_init(pthread_attr_t *);
int pthread_attr_setdetachstate(pthread_attr_t *, int);
int pthread_attr_setguardsize(pthread_attr_t *, size_t);
int pthread_attr_setinheritsched(pthread_attr_t *, int);
int pthread_attr_setschedparam(pthread_attr_t *,
          const struct sched_param *);
int pthread_attr_setschedpolicy(pthread_attr_t *, int);
int pthread_attr_setscope(pthread_attr_t *, int);
int pthread_attr_setstackaddr(pthread_attr_t *, void *);
int pthread_attr_setstacksize(pthread_attr_t *, size_t);
int pthread_cancel(pthread_t);
int pthread_cond_broadcast(pthread_cond_t *);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_condattr_destroy(pthread_condattr_t *);
int pthread_condattr_getpshared(const pthread_condattr_t *, int *);
int pthread_condattr_init(pthread_condattr_t *);
int pthread_condattr_setpshared(pthread_condattr_t *, int);

int pthread_detach(pthread_t);
int pthread_equal(pthread_t, pthread_t);
int pthread_getconcurrency(void);
int pthread_getschedparam(pthread_t, int *, struct sched_param *);
void *pthread_getspecific(pthread_key_t);
int pthread_key_create(pthread_key_t *, void (*)(void *));
int pthread_key_delete(pthread_key_t);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *,
          int *);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int pthread_once(pthread_once_t *, void (*)(void));
int pthread_rwlock_destroy(pthread_rwlock_t *);
int pthread_rwlock_init(pthread_rwlock_t *,
          const pthread_rwlockattr_t *);
int pthread_rwlock_rdlock(pthread_rwlock_t *);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int pthread_rwlock_trywrlock(pthread_rwlock_t *);
int pthread_rwlock_unlock(pthread_rwlock_t *);
int pthread_rwlock_wrlock(pthread_rwlock_t *);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *,
          int *);
int pthread_rwlockattr_init(pthread_rwlockattr_t *);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
int pthread_setcancelstate(int, int *);
int pthread_setcanceltype(int, int *);
int pthread_setconcurrency(int);
int pthread_setschedparam(pthread_t, int ,
          const struct sched_param *);
int pthread_setspecific(pthread_key_t, const void *);
void pthread_testcancel(void);

}

void check();
#endif