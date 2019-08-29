#ifndef __NEWFUZZER_H__
#define __NEWFUZZER_H__

#include "fuzzer.h"
#include "classlist.h"
#include "mymemory.h"
#include "stl-model.h"

typedef HashTable<FuncInst *, ModelAction *, uintptr_t, 0> inst_act_map_t;

class NewFuzzer : public Fuzzer {
public:
	NewFuzzer();
	int selectWrite(ModelAction *read, SnapVector<ModelAction *>* rf_set);
	Predicate * selectBranch(thread_id_t tid, Predicate * curr_pred, FuncInst * read_inst);
	Predicate * get_selected_child_branch(thread_id_t tid);
	bool prune_writes(thread_id_t tid, Predicate * pred, SnapVector<ModelAction *> * rf_set, inst_act_map_t * inst_act_map);

	Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction *sleep);
	bool shouldWake(const ModelAction *sleep);

	void register_engine(ModelHistory * history, ModelExecution * execution);

	SNAPSHOTALLOC
private:
	ModelHistory * history;
	ModelExecution * execution;

	SnapVector<ModelAction *> thrd_last_read_act;
	SnapVector<Predicate *> thrd_curr_pred;
	SnapVector<Predicate *> thrd_selected_child_branch;
	SnapVector< SnapVector<ModelAction *> *> thrd_pruned_writes;
};

#endif /* end of __NEWFUZZER_H__ */
