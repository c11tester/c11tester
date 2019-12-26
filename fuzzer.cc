#include "fuzzer.h"
#include <stdlib.h>
#include "threads-model.h"
#include "model.h"
#include "action.h"

int Fuzzer::selectWrite(ModelAction *read, SnapVector<ModelAction *> * rf_set) {
	int random_index = random() % rf_set->size();
	return random_index;
}

Thread * Fuzzer::selectThread(int * threadlist, int numthreads) {
	int random_index = random() % numthreads;
	int thread = threadlist[random_index];
	thread_id_t curr_tid = int_to_id(thread);
	return model->get_thread(curr_tid);
}

Thread * Fuzzer::selectNotify(action_list_t * waiters) {
	int numwaiters = waiters->size();
	int random_index = random() % numwaiters;
	sllnode<ModelAction*> * it = waiters->begin();
	while(random_index--)
		it=it->getNext();
	Thread *thread = model->get_thread(it->getVal());
	waiters->erase(it);
	return thread;
}

bool Fuzzer::shouldSleep(const ModelAction *sleep) {
	return true;
}

bool Fuzzer::shouldWake(const ModelAction *sleep) {
	struct timespec currtime;
	clock_gettime(CLOCK_MONOTONIC, &currtime);
	uint64_t lcurrtime = currtime.tv_sec * 1000000000 + currtime.tv_nsec;

	return ((sleep->get_time()+sleep->get_value()) < lcurrtime);
}

bool Fuzzer::shouldWait(const ModelAction * act)
{
	return random() & 1;
}
