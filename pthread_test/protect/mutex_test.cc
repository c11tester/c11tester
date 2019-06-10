#include <stdio.h> 
 
#include "threads.h" 
#include "librace.h" 
#include "stdatomic.h" 
#include <pthread.h>

int shareddata;  
pthread_mutex_t m;

static void* a(void *obj) 
{ 
        int i; 
        for(i=0;i<2;i++) { 
                if ((i%2)==0) { 
                        pthread_mutex_lock(&m);
                        store_32(&shareddata,(unsigned int)i); 
			printf("shareddata: %d\n", shareddata);
                        pthread_mutex_unlock(&m);
                } else { 
                        while(!pthread_mutex_trylock(&m))
                                thrd_yield(); 
                        store_32(&shareddata,(unsigned int)i); 
			printf("shareddata: %d\n", shareddata);
                        pthread_mutex_unlock(&m);
                } 
        } 
} 
 
int user_main(int argc, char **argv) 
{ 
        thrd_t t1, t2; 
	pthread_mutex_init(&m, NULL);

        thrd_create(&t1, (thrd_start_t)&a, NULL);
        thrd_create(&t2, (thrd_start_t)&a, NULL);

        thrd_join(t1); 
        thrd_join(t2); 
        return 0; 
}
