#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_short x;
atomic_short y;

static void a(void *obj)
{
	short desire = 315;
	short expected = 0;
	short * pt = &expected;

	printf("expected was %d, but x is %d\n", expected, x);

	short v1 = atomic_compare_exchange_weak_explicit(&x, pt, desire, memory_order_relaxed, memory_order_acquire);
	printf("Then v1 = %d, x = %d\n", v1, x);
	printf("expected: %d\n", expected);
/*
        short v1 = atomic_exchange_explicit(&x, 8, memory_order_relaxed);
	short v2 = atomic_exchange_explicit(&x, -10, memory_order_relaxed);
	short v3 = atomic_load_explicit(&x, memory_order_relaxed);
	printf("v1 = %d, v2 = %d, v3 = %d\n", v1, v2, v3);
*/
}

static void b(void *obj)
{
	int v3=atomic_fetch_add_explicit(&y, 2, memory_order_relaxed);
	int v4=atomic_fetch_add_explicit(&x, -5, memory_order_relaxed);
	printf("v3 = %d, v4=%d\n", v3, v4);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
//	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
//	thrd_join(t2);

	return 0;
}
