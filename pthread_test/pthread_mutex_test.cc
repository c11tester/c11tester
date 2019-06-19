#include <stdio.h>
#include <pthread.h>
#define NTHREADS 2
void *thread_function(void *);
//pthread_mutex_t mutex1; 

int counter=0;

class Box {
public:
//	int counter;

	static Box& getInstance() {
		static Box instance;
		return instance;
	}

	void addone() {
		pthread_mutex_lock(&pool_mutex);
		counter++;
		pthread_mutex_unlock(&pool_mutex);
	}

private:
	Box() {	
		pthread_mutex_init(&pool_mutex, NULL); 
		counter = 0;
	}
	pthread_mutex_t pool_mutex;
};

int user_main(int argv, char **argc)
{
//   void *dummy = NULL;
//   pthread_mutex_init(&mutex1, NULL); /* PTHREAD_MUTEX_INITIALIZER;*/

//   pthread_t thread_id[NTHREADS];
//   int i, j;

   Box::getInstance().addone();

/*   for(i=0; i < NTHREADS; i++)
   {
      pthread_create( &thread_id[i], NULL, &thread_function, NULL );
   }

   for(j=0; j < NTHREADS; j++)
   {
      pthread_join( thread_id[j], NULL); 
   }
*/
 
   printf("Final counter value: %d\n", counter);
   /*
   for (i=0;i<NTHREADS; i++) {
      printf("id %ld\n", thread_id[i]);
   }*/
   return 0;
}

void *thread_function(void *dummyPtr)
{
//   printf("Thread number %ld\n", pthread_self());
   Box::getInstance().addone();
//   pthread_mutex_lock( &mutex1 );
//   Box::getInstance().counter++;
//   pthread_mutex_unlock( &mutex1 );
   return NULL;
}
