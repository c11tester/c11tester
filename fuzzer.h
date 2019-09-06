#ifndef FUZZER_H
#define FUZZER_H
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"
#include "threads-model.h"

class Fuzzer {
public:
	Fuzzer() {}
	virtual int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	virtual Predicate * get_selected_child_branch(thread_id_t tid) = 0;
	virtual bool has_paused_threads() { return false; }
	virtual Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction *sleep);
	bool shouldWake(const ModelAction *sleep);
	virtual bool shouldWait(const ModelAction *wait) = 0;
	virtual void register_engine(ModelHistory * history, ModelExecution * execution) = 0;
	SNAPSHOTALLOC
private:
};
#endif
