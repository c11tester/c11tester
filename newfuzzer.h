#ifndef __NEWFUZZER_H__
#define __NEWFUZZER_H__

#include "fuzzer.h"
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"

class NewFuzzer : public Fuzzer {
public:
	NewFuzzer();
	int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	void selectBranch(int thread_id, Predicate * curr_pred, FuncInst * read_inst);

	Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction *sleep);
	bool shouldWake(const ModelAction *sleep);

	void register_engine(ModelHistory * history, ModelExecution * execution);

	MEMALLOC
private:
	ModelHistory * history;
	ModelExecution * execution;

	SnapVector<ModelAction *> thrd_last_read_act;
	SnapVector<Predicate *> thrd_curr_pred;
	SnapVector<Predicate *> thrd_selected_child_branch;
};

#endif /* end of __NEWFUZZER_H__ */
