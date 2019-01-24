/**
 * @file pthread.h
 * @brief C11 pthread.h interface header
 */
#ifndef PTHREAD_H
#define PTHREAD_H 1

#include <threads.h>

typedef void *(*pthread_start_t)(void *);

struct pthread_params {
    pthread_start_t func;
    void *arg;
};

int pthread_create(pthread_t *, const pthread_attr_t *,
          void *(*start_routine) (void *), void * arg);
void pthread_exit(void *);
int pthread_join(pthread_t, void **);

int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);

int user_main(int, char**);

#endif
