#ifndef __NEWFUZZER_H__
#define __NEWFUZZER_H__

#include "fuzzer.h"
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"

class NewFuzzer : public Fuzzer {
public:
	NewFuzzer() {}
	int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction *sleep);
	bool shouldWake(const ModelAction *sleep);
	MEMALLOC
private:
};

#endif /* end of __NEWFUZZER_H__ */
