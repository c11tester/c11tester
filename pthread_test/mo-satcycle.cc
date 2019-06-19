/**
 * @file mo-satcycle.cc
 * @brief MO satisfaction cycle test
 *
 * This program has a peculiar behavior which is technically legal under the
 * current C++ memory model but which is a result of a type of satisfaction
 * cycle. We use this as justification for part of our modifications to the
 * memory model when proving our model-checker's correctness.
 */

#include <atomic>
#include <pthread.h>
#include <stdio.h>

#include "model-assert.h"

using namespace std;

atomic_int x, y;
int r0, r1, r2, r3; /* "local" variables */

static void *a(void *obj)
{
	y.store(10, memory_order_relaxed);
	x.store(1, memory_order_release);
	return NULL;
}

static void *b(void *obj)
{
	r0 = x.load(memory_order_relaxed);
	r1 = x.load(memory_order_acquire);
	y.store(11, memory_order_relaxed);
	return NULL;
}

static void *c(void *obj)
{
	r2 = y.load(memory_order_relaxed);
	r3 = y.load(memory_order_relaxed);
	if (r2 == 11 && r3 == 10)
		x.store(0, memory_order_relaxed);
	return NULL;
}

int user_main(int argc, char **argv)
{
	pthread_t t1, t2, t3;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating 3 threads\n");
	pthread_create(&t1,NULL, &a, NULL);
	pthread_create(&t2,NULL, &b, NULL);
	pthread_create(&t3,NULL, &c, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	pthread_join(t3,NULL);
	printf("Main thread is finished\n");

	/*
	 * This condition should not be hit because it only occurs under a
	 * satisfaction cycle
	 */
	bool cycle = (r0 == 1 && r1 == 0 && r2 == 11 && r3 == 10);
	MODEL_ASSERT(!cycle);

	return 0;
}
