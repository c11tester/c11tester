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
	Predicate * get_selected_child_branch(thread_id_t tid);
	bool has_paused_threads();
	void notify_paused_thread(Thread * thread);

	Thread * selectThread(int * threadlist, int numthreads);
	Thread * selectNotify(action_list_t * waiters);
	bool shouldSleep(const ModelAction * sleep);
	bool shouldWake(const ModelAction * sleep);
	bool shouldWait(const ModelAction * wait);

	void register_engine(ModelHistory * history, ModelExecution * execution);

	SNAPSHOTALLOC
private:
	ModelHistory * history;
	ModelExecution * execution;

	SnapVector<ModelAction *> thrd_last_read_act;
	SnapVector<FuncInst *> thrd_last_func_inst;

	SnapVector<Predicate *> thrd_selected_child_branch;
	SnapVector< SnapVector<ModelAction *> *> thrd_pruned_writes;

	bool prune_writes(thread_id_t tid, Predicate * pred, SnapVector<ModelAction *> * rf_set, inst_act_map_t * inst_act_map);
	Predicate * selectBranch(thread_id_t tid, Predicate * curr_pred, FuncInst * read_inst);
	int choose_index(SnapVector<Predicate *> * branches, uint32_t numerator);

	/* The set of Threads put to sleep by NewFuzzer because no writes in rf_set satisfies the selected predicate. Only used by selectWrite.
	 */
	SnapVector<Thread *> paused_thread_list;
	HashTable<Thread *, int, uintptr_t, 0> paused_thread_table;
	HashTable<Predicate *, bool, uintptr_t, 0> failed_predicates;

	void conditional_sleep(Thread * thread);
	void wake_up_paused_threads(int * threadlist, int * numthreads);

	bool find_threads(ModelAction * pending_read);
};

#endif /* end of __NEWFUZZER_H__ */
