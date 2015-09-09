/**
 * @file iriw.cc
 * @brief Independent read and independent write test
 */

#include <atomic>
#include <threads.h>
#include <stdio.h>

#include "wildcard.h"
#include "model-assert.h"

using namespace std;

atomic_int x, y;
int r1, r2, r3, r4; /* "local" variables */

static void a(void *obj)
{
	x.store(1, memory_order_seq_cst);
}

static void b(void *obj)
{
	y.store(1, memory_order_seq_cst);
}

static void c(void *obj)
{
	r1 = x.load(memory_order_acquire);
	r2 = y.load(memory_order_seq_cst);
}

static void d(void *obj)
{
	r3 = y.load(memory_order_acquire);
	r4 = x.load(memory_order_seq_cst);
}


int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3, t4;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating 4 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);
	thrd_create(&t4, (thrd_start_t)&d, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	thrd_join(t4);
	printf("Main thread is finished\n");

	/*
	 * This condition should not be hit if the execution is SC */
	bool sc = (r1 == 1 && r2 == 0 && r3 == 1 && r4 == 0);
	printf("r1 = %d, r2 = %d, r3 = %d and r4 = %d\n", r1, r2, r3, r4);
	MODEL_ASSERT(!sc);
	return 0;
}
