#include "fuzzer.h"
#include <stdlib.h>
#include "threads-model.h"
#include "model.h"

int Fuzzer::selectWrite(ModelAction *read, SnapVector<ModelAction *> * rf_set) {
	int random_index = random() % rf_set->size();
	return random_index;
}

Thread * Fuzzer::selectThread(Node *n, int * threadlist, int numthreads) {
	int random_index = random() % numthreads;
	int thread = threadlist[random_index];
	thread_id_t curr_tid = int_to_id(thread);
	return model->get_thread(curr_tid);
}

Thread * Fuzzer::selectNotify(action_list_t * waiters) {
	int numwaiters = waiters->size();
	int random_index = random() % numwaiters;
	action_list_t::iterator it = waiters->begin();
	advance(it, random_index);
	Thread *thread = model->get_thread(*it);
	waiters->erase(it);
	return thread;
}
