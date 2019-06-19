/**
 * @file iriw.cc
 * @brief Independent read and independent write test
 */

#include <atomic>
#include <pthread.h>
#include <stdio.h>
#include <iostream>

#define N 4
//#include "wildcard.h"
//#include "model-assert.h"

using namespace std;

atomic<int> x(1);
atomic<int> y(1);

int r1, r2, r3, r4; /* "local" variables */

static void *a(void *obj)
{
	x.store(1, memory_order_relaxed);
	y.store(1, memory_order_relaxed);

	return new int(1);
}


static void *b(void *obj)
{
	y.store(1, memory_order_relaxed);
	
	return new int(2);
}

static void *c(void *obj)
{
	r1 = x.load(memory_order_acquire);
	r2 = y.load(memory_order_relaxed);

	return new int(3);
}

static void *d(void *obj)
{
	r3 = y.load(memory_order_acquire);
	r4 = x.load(memory_order_relaxed);

	return new int(4);
}


int main(int argc, char **argv)
{
	printf("Main thread starts\n");

	x.store(2, memory_order_relaxed);
	y.store(2, memory_order_relaxed);

	r1 = x.load(memory_order_relaxed);
	printf("%d\n", r1);

//	x.compare_exchange_strong(r1, r2);
//	r3 = x.load(memory_order_relaxed);

//	printf("%d\n", r3);

	printf("Main thread is finished\n");

	return 0;
}
