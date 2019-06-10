/**
 * @file iriw.cc
 * @brief Independent read and independent write test
 */

#include <atomic>
#include <pthread.h>
#include <stdio.h>

#define N 14
//#include "wildcard.h"
//#include "model-assert.h"

using namespace std;

std::atomic_int x, y;
int r1, r2, r3, r4; /* "local" variables */

static void *a(void *obj)
{
//	x.store(1, memory_order_seq_cst);
	return NULL;
}


static void *b(void *obj)
{
	y.store(1, memory_order_seq_cst);
	return NULL;
}

static void *c(void *obj)
{
	r1 = x.load(memory_order_acquire);
	r2 = y.load(memory_order_seq_cst);
	return NULL;
}

static void *d(void *obj)
{
	r3 = y.load(memory_order_acquire);
	r4 = x.load(memory_order_seq_cst);
	return NULL;
}


int user_main(int argc, char **argv)
{
	pthread_t threads[20];

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating %d threads\n", N);

	for (int i = 0; i< N; i++)
		pthread_create(&threads[i],NULL, &a, NULL);

	for (int i=0; i<N; i++)
		printf("thread id: %ld\n", threads[i]);

	for (int i = 0; i< N; i++)
		pthread_join( threads[i],NULL);

	printf("Main thread is finished\n");

	/*
	 * This condition should not be hit if the execution is SC */
//	bool sc = (r1 == 1 && r2 == 0 && r3 == 1 && r4 == 0);
//	printf("r1 = %d, r2 = %d, r3 = %d and r4 = %d\n", r1, r2, r3, r4);
//	MODEL_ASSERT(!sc);

	return 0;
}
