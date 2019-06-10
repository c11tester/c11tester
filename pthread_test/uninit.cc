/**
 * @file uninit.cc
 * @brief Uninitialized loads test
 *
 * This is a test of the "uninitialized loads" code. While we don't explicitly
 * initialize y, this example's synchronization pattern should guarantee we
 * never see it uninitialized.
 */
#include <stdio.h>
#include <pthread.h>
#include <atomic>

//#include "librace.h"

std::atomic_int x;
std::atomic_int y;

static void *a(void *obj)
{
	int flag = x.load(std::memory_order_acquire);
	printf("flag: %d\n", flag);
	if (flag == 2)
		printf("Load: %d\n", y.load(std::memory_order_relaxed));
	return NULL;
}

static void *b(void *obj)
{
	printf("fetch_add: %d\n", x.fetch_add(1, std::memory_order_relaxed));
	return NULL;
}

static void *c(void *obj)
{
	y.store(3, std::memory_order_relaxed);
	x.store(1, std::memory_order_release);
	return NULL;
}

int user_main(int argc, char **argv)
{
	pthread_t t1, t2, t3;

	std::atomic_init(&x, 0);

	printf("Main thread: creating 3 threads\n");
	pthread_create(&t1,NULL, &a, NULL);
	pthread_create(&t2,NULL, &b, NULL);
	pthread_create(&t3,NULL, &c, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	pthread_join(t3,NULL);
	printf("Main thread is finished\n");

	return 0;
}
