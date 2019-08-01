/**
 * @file pthread.h
 * @brief C11 pthread.h interface header
 */
#ifndef PTHREAD_H
#define PTHREAD_H

#include <threads.h>
#include <sched.h>
#include <pthread.h>

typedef void *(*pthread_start_t)(void *);

struct pthread_params {
	pthread_start_t func;
	void *arg;
};

extern "C" {
int user_main(int, char**);
}

#endif
