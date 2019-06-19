/**
 * @file iriw.cc
 * @brief Independent read and independent write test
 */

#include <atomic>
#include <pthread.h>
#include <stdio.h>

#include "wildcard.h"
#include "model-assert.h"

using namespace std;

atomic_int x, y;
int r1, r2, r3, r4; /* "local" variables */

static void *a(void *obj)
{
	x.store(1, memory_order_seq_cst);
	return new int(1);
}

static void *b(void *obj)
{
	y.store(1, memory_order_seq_cst);
	return new int(2);
}

static void *c(void *obj)
{
	r1 = x.load(memory_order_acquire);
	r2 = y.load(memory_order_seq_cst);
	return new int(3);
}

static void *d(void *obj)
{
	r3 = y.load(memory_order_acquire);
	r4 = x.load(memory_order_seq_cst);
	return new int(4);
}


int user_main(int argc, char **argv)
{
	pthread_t t1, t2, t3, t4;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating 4 threads\n");
	pthread_create(&t1,NULL, &a, NULL);
	pthread_create(&t2,NULL, &b, NULL);
	pthread_create(&t3,NULL, &c, NULL);
	pthread_create(&t4,NULL, &d, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	pthread_join(t3,NULL);
	pthread_join(t4,NULL);
	printf("Main thread is finished\n");

	/*
	 * This condition should not be hit if the execution is SC */
	bool sc = (r1 == 1 && r2 == 0 && r3 == 1 && r4 == 0);
	printf("r1 = %d, r2 = %d, r3 = %d and r4 = %d\n", r1, r2, r3, r4);
	MODEL_ASSERT(!sc);

	return 0;
}
