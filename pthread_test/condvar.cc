#include <stdio.h>

#include "pthread.h"
#include "librace.h"
#include "stdatomic.h"
#include "mutex.h"
#include <condition_variable>

cdsc::mutex * m;
cdsc::condition_variable *v;
int shareddata;

static void *a(void *obj)
{
	m->lock();
	while(load_32(&shareddata)==0)
		v->wait(*m);
	m->unlock();
	return NULL;
}

static void *b(void *obj)
{
	m->lock();
	store_32(&shareddata, (unsigned int) 1);
	v->notify_all();
	m->unlock();
	return NULL;
}

int user_main(int argc, char **argv)
{
	pthread_t t1, t2;
	store_32(&shareddata, (unsigned int) 0);
	m=new cdsc::mutex();
	v=new cdsc::condition_variable();

	pthread_create(&t1,NULL, &a, NULL);
	pthread_create(&t2,NULL, &b, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	return 0;
}
