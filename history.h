#ifndef __HISTORY_H__
#define __HISTORY_H__

#include "stl-model.h"
#include "common.h"
#include "classlist.h"
#include "hashtable.h"
#include "threads-model.h"

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
	HashTable<void *, value_set_t *, uintptr_t, 4> * getWriteHistory() { return write_history; }
	void update_loc_func_nodes_map(void * location, FuncNode * node);
	void update_loc_wr_func_nodes_map(void * location, FuncNode * node);

	void add_waiting_write(ConcretePredicate * concrete);
	void remove_waiting_write(thread_id_t tid);
	void check_waiting_write(ModelAction * write_act);
	SnapVector<ConcretePredicate *> * getThrdWaitingWrite() { return thrd_waiting_write; }

	SnapVector<inst_act_map_t *> * getThrdInstActMap(uint32_t func_id);

	void set_new_exec_flag();
	void dump_func_node_graph();
	void print_func_node();

	MEMALLOC
private:
	uint32_t func_counter;

	/* Map function names to integer ids */
	HashTable<const char *, uint32_t, uintptr_t, 4, model_malloc, model_calloc, model_free> func_map;

	/* Map integer ids to function names */
	ModelVector<const char *> func_map_rev;

	ModelVector<FuncNode *> func_nodes;

	/* Map a location to a set of values that have been written to it */
	HashTable<void *, value_set_t *, uintptr_t, 4> * write_history;

	/* Map a location to FuncNodes that may read from it */
	HashTable<void *, SnapList<FuncNode *> *, uintptr_t, 0> * loc_func_nodes_map;

	/* Map a location to FuncNodes that may write to it */
	HashTable<void *, SnapList<FuncNode *> *, uintptr_t, 0> * loc_wr_func_nodes_map;

	HashTable<void *, SnapVector<ConcretePredicate *> *, uintptr_t, 0> * loc_waiting_writes_map;
	SnapVector<ConcretePredicate *> * thrd_waiting_write;

	/* A run-time map from FuncInst to ModelAction per each FuncNode, per each thread.
	 * Manipulated by FuncNode, and needed by NewFuzzer */
	HashTable<uint32_t, SnapVector<inst_act_map_t *> *, int, 0> * func_inst_act_maps;

	bool skip_action(ModelAction * act, SnapList<ModelAction *> * curr_act_list);
};

#endif	/* __HISTORY_H__ */
