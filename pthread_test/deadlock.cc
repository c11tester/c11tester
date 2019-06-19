#include <stdio.h>
#include <pthread.h>

#include "librace.h"

pthread_mutex_t x;
pthread_mutex_t y;
uint32_t shared = 0;

static void *a(void *obj)
{
	pthread_mutex_lock(&x);
	pthread_mutex_lock(&y);
	printf("shared = %u\n", load_32(&shared));
	pthread_mutex_unlock(&y);
	pthread_mutex_unlock(&x);
	return NULL;
}

static void *b(void *obj)
{
	pthread_mutex_lock(&y);
	pthread_mutex_lock(&x);
	store_32(&shared, 16);
	printf("write shared = 16\n");
	pthread_mutex_unlock(&x);
	pthread_mutex_unlock(&y);
	return NULL;
}

int user_main(int argc, char **argv)
{
	pthread_t t1, t2;

	pthread_mutex_init(&x, NULL);
	pthread_mutex_init(&y, NULL);

	printf("Main thread: creating 2 threads\n");
	pthread_create(&t1,NULL, &a, NULL);
	pthread_create(&t2,NULL, &b, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	printf("Main thread is finished\n");

	return 0;
}
