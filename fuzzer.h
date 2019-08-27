#ifndef FUZZER_H
#define FUZZER_H
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"

class Fuzzer {
public:
	Fuzzer() {}
	virtual int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction *sleep);
	bool shouldWake(const ModelAction *sleep);
	virtual void register_engine(ModelHistory * history, ModelExecution * execution) {}
	MEMALLOC
private:
};
#endif
