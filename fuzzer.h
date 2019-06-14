#ifndef FUZZER_H
#define FUZZER_H
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"

class Fuzzer {
public:
	Fuzzer() {}
	int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	Thread * selectThread(Node *n, int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	MEMALLOC
private:
};
#endif
