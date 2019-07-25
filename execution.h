/** @file execution.h
 *  @brief Model-checker core
 */

#ifndef __EXECUTION_H__
#define __EXECUTION_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "hashtable.h"
#include "config.h"
#include "modeltypes.h"
#include "stl-model.h"
#include "params.h"
#include "mypthread.h"
#include "mutex.h"
#include <condition_variable>
#include "classlist.h"

typedef SnapList<ModelAction *> action_list_t;

struct PendingFutureValue {
	PendingFutureValue(ModelAction *writer, ModelAction *reader) :
		writer(writer), reader(reader)
	{ }
	const ModelAction *writer;
	ModelAction *reader;
};

/** @brief The central structure for model-checking */
class ModelExecution {
public:
	ModelExecution(ModelChecker *m, Scheduler *scheduler);
	~ModelExecution();

	struct model_params * get_params() const { return params; }
	void setParams(struct model_params * _params) {params = _params;}

	Thread * take_step(ModelAction *curr);

	void print_summary() const;
#if SUPPORT_MOD_ORDER_DUMP
	void dumpGraph(char *filename) const;
#endif

	void add_thread(Thread *t);
	Thread * get_thread(thread_id_t tid) const;
	Thread * get_thread(const ModelAction *act) const;

	uint32_t get_pthread_counter() { return pthread_counter; }
	void incr_pthread_counter() { pthread_counter++; }
	Thread * get_pthread(pthread_t pid);

	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;

	thread_id_t get_next_id();
	unsigned int get_num_threads() const;

	ClockVector * get_cv(thread_id_t tid) const;
	ModelAction * get_parent_action(thread_id_t tid) const;
	bool isfeasibleprefix() const;

	ModelAction * get_last_action(thread_id_t tid) const;

	bool check_action_enabled(ModelAction *curr);

	bool assert_bug(const char *msg);

	bool have_bug_reports() const;
	bool have_fatal_bug_reports() const;

	SnapVector<bug_message *> * get_bugs() const;

	bool has_asserted() const;
	void set_assert();
	bool is_complete_execution() const;

	void print_infeasibility(const char *prefix) const;
	bool is_infeasible() const;
	bool is_deadlocked() const;

	action_list_t * get_action_trace() { return &action_trace; }
	Fuzzer * getFuzzer();
	CycleGraph * const get_mo_graph() { return mo_graph; }
	HashTable<pthread_cond_t *, cdsc::snapcondition_variable *, uintptr_t, 4> * getCondMap() {return &cond_map;}
	HashTable<pthread_mutex_t *, cdsc::snapmutex *, uintptr_t, 4> * getMutexMap() {return &mutex_map;}
	ModelAction * check_current_action(ModelAction *curr);

	SnapVector<func_id_list_t *> * get_thrd_func_list() { return &thrd_func_list; }
	SnapVector< SnapList<func_inst_list_t *> *> * get_thrd_func_inst_lists() { return &thrd_func_inst_lists; }

	SNAPSHOTALLOC
private:
	int get_execution_number() const;

	ModelChecker *model;

	struct model_params * params;

	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler * const scheduler;

	bool mo_may_allow(const ModelAction *writer, const ModelAction *reader);
	void set_bad_synchronization();
	bool should_wake_up(const ModelAction *curr, const Thread *thread) const;
	void wake_up_sleeping_actions(ModelAction *curr);
	modelclock_t get_next_seq_num();

	bool next_execution();
	bool initialize_curr_action(ModelAction **curr);
	void process_read(ModelAction *curr, SnapVector<ModelAction *> * rf_set);
	void process_write(ModelAction *curr);
	bool process_fence(ModelAction *curr);
	bool process_mutex(ModelAction *curr);

	bool process_thread_action(ModelAction *curr);
	void read_from(ModelAction *act, ModelAction *rf);
	bool synchronize(const ModelAction *first, ModelAction *second);

	void add_action_to_lists(ModelAction *act);
	void add_normal_write_to_lists(ModelAction *act);
	void add_write_to_lists(ModelAction *act);
	ModelAction * get_last_fence_release(thread_id_t tid) const;
	ModelAction * get_last_seq_cst_write(ModelAction *curr) const;
	ModelAction * get_last_seq_cst_fence(thread_id_t tid, const ModelAction *before_fence) const;
	ModelAction * get_last_unlock(ModelAction *curr) const;
	SnapVector<ModelAction *> * build_may_read_from(ModelAction *curr);
	ModelAction * process_rmw(ModelAction *curr);

	bool r_modification_order(ModelAction *curr, const ModelAction *rf, SnapVector<const ModelAction *> *priorset, bool *canprune);
	void w_modification_order(ModelAction *curr);
	ClockVector * get_hb_from_write(ModelAction *rf) const;
	ModelAction * get_uninitialized_action(ModelAction *curr) const;
	ModelAction * convertNonAtomicStore(void*);

	action_list_t action_trace;
	SnapVector<Thread *> thread_map;
	SnapVector<Thread *> pthread_map;
	uint32_t pthread_counter;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> obj_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> condvar_waiters_map;

	HashTable<const void *, SnapVector<action_list_t> *, uintptr_t, 4> obj_thrd_map;

	HashTable<const void *, SnapVector<action_list_t> *, uintptr_t, 4> obj_wr_thrd_map;

	HashTable<const void *, ModelAction *, uintptr_t, 4> obj_last_sc_map;


	HashTable<pthread_mutex_t *, cdsc::snapmutex *, uintptr_t, 4> mutex_map;
	HashTable<pthread_cond_t *, cdsc::snapcondition_variable *, uintptr_t, 4> cond_map;

	/**
	 * List of pending release sequences. Release sequences might be
	 * determined lazily as promises are fulfilled and modification orders
	 * are established. Each entry in the list may only be partially
	 * filled, depending on its pending status.
	 */

	SnapVector<ModelAction *> thrd_last_action;
	SnapVector<ModelAction *> thrd_last_fence_release;

	/** A special model-checker Thread; used for associating with
	 *  model-checker-related ModelAcitons */
	Thread *model_thread;

	/** Private data members that should be snapshotted. They are grouped
	 * together for efficiency and maintainability. */
	struct model_snapshot_members * const priv;

	/**
	 * @brief The modification order graph
	 *
	 * A directed acyclic graph recording observations of the modification
	 * order on all the atomic objects in the system. This graph should
	 * never contain any cycles, as that represents a violation of the
	 * memory model (total ordering). This graph really consists of many
	 * disjoint (unconnected) subgraphs, each graph corresponding to a
	 * separate ordering on a distinct object.
	 *
	 * The edges in this graph represent the "ordered before" relation,
	 * such that <tt>a --> b</tt> means <tt>a</tt> was ordered before
	 * <tt>b</tt>.
	 */
	CycleGraph * const mo_graph;

	Fuzzer * fuzzer;

	Thread * action_select_next_thread(const ModelAction *curr) const;

	/* thrd_func_list stores a list of function ids for each thread.
	 * Each element in thrd_func_list stores the functions that
	 * thread i has entered and yet to exit from
	 *
	 * This data structure is handled by ModelHistory
	 */
	SnapVector< func_id_list_t * > thrd_func_list;

	/* Keeps track of atomic actions that thread i has performed in some
	 * function. Index of SnapVector is thread id. SnapList simulates
	 * the call stack.
	 *
	 * This data structure is handled by ModelHistory
	 */
	SnapVector< SnapList< func_inst_list_t *> *> thrd_func_inst_lists;

#ifdef TLS
public:
	char * getTLSBase() {return tls_base;}
	uintptr_t getTLSAddr() {return tls_addr;}
	uintptr_t getTLSSize() {if (!tls_base) initTLS();return tls_size;}
	uintptr_t getThdDescSize() {return thd_desc_size;}
private:
	void initTLS();
	char * tls_base;
	uintptr_t tls_addr;
	uintptr_t tls_size;
	uintptr_t thd_desc_size;
#endif
};

#endif	/* __EXECUTION_H__ */
