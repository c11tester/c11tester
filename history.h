#ifndef __HISTORY_H__
#define __HISTORY_H__

#include "common.h"
#include "classlist.h"
#include "hashtable.h"
#include "threads-model.h"

#define INIT_SEQ_NUMBER 0xffffffff

class ModelHistory {
public:
	ModelHistory();
	~ModelHistory();

	void enter_function(const uint32_t func_id, thread_id_t tid);
	void exit_function(const uint32_t func_id, thread_id_t tid);

	uint32_t get_func_counter() { return func_counter; }
	void incr_func_counter() { func_counter++; }

	void resize_func_nodes(uint32_t max_func_id);
	void process_action(ModelAction *act, thread_id_t tid);

	HashTable<const char *, uint32_t, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncMap() { return &func_map; }
	ModelVector<const char *> * getFuncMapRev() { return &func_map_rev; }

	ModelVector<FuncNode *> * getFuncNodes() { return &func_nodes; }
	FuncNode * get_func_node(uint32_t func_id);
	FuncNode * get_curr_func_node(thread_id_t tid);

	void update_write_history(void * location, uint64_t write_val);
	HashTable<void *, value_set_t *, uintptr_t, 0> * getWriteHistory() { return write_history; }
	void update_loc_rd_func_nodes_map(void * location, FuncNode * node);
	void update_loc_wr_func_nodes_map(void * location, FuncNode * node);
	SnapVector<FuncNode *> * getRdFuncNodes(void * location);
	SnapVector<FuncNode *> * getWrFuncNodes(void * location);

	void add_waiting_write(ConcretePredicate * concrete);
	void remove_waiting_write(thread_id_t tid);
	void check_waiting_write(ModelAction * write_act);
	SnapVector<ConcretePredicate *> * getThrdWaitingWrite() { return thrd_waiting_write; }

	WaitObj * getWaitObj(thread_id_t tid);
	void add_waiting_thread(thread_id_t self_id, thread_id_t waiting_for_id, FuncNode * target_node, int dist);
	void remove_waiting_thread(thread_id_t tid);
	void stop_waiting_for_node(thread_id_t self_id, thread_id_t waiting_for_id, FuncNode * target_node);

	void set_new_exec_flag();
	void dump_func_node_graph();
	void print_func_node();
	void print_waiting_threads();

	MEMALLOC
private:
	uint32_t func_counter;
	modelclock_t last_seq_number;

	/* Map function names to integer ids */
	HashTable<const char *, uint32_t, uintptr_t, 4, model_malloc, model_calloc, model_free> func_map;

	/* Map integer ids to function names */
	ModelVector<const char *> func_map_rev;

	ModelVector<FuncNode *> func_nodes;

	/* Map a location to a set of values that have been written to it */
	HashTable<void *, value_set_t *, uintptr_t, 0> * write_history;

	/* Map a location to FuncNodes that may read from it */
	HashTable<void *, SnapVector<FuncNode *> *, uintptr_t, 0> * loc_rd_func_nodes_map;

	/* Map a location to FuncNodes that may write to it */
	HashTable<void *, SnapVector<FuncNode *> *, uintptr_t, 0> * loc_wr_func_nodes_map;

	HashTable<void *, SnapVector<ConcretePredicate *> *, uintptr_t, 0> * loc_waiting_writes_map;

	/* thrd_func_list stores a list of function ids for each thread.
	 * Each element in thrd_func_list stores the functions that
	 * thread i has entered and yet to exit from
	 */
	SnapVector<func_id_list_t> * thrd_func_list;
	SnapVector<uint32_t> * thrd_last_entered_func;

	/* The write values each paused thread is waiting for */
	SnapVector<ConcretePredicate *> * thrd_waiting_write;
	SnapVector<WaitObj *> * thrd_wait_obj;

	bool skip_action(ModelAction * act);
	void monitor_waiting_thread(uint32_t func_id, thread_id_t tid);
	void monitor_waiting_thread_counter(thread_id_t tid);

};

#endif	/* __HISTORY_H__ */
