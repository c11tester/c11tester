#ifndef __HISTORY_H__
#define __HISTORY_H__

#include "stl-model.h"
#include "common.h"
#include "hashtable.h"
#include "hashset.h"
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
//	uint64_t query_last_read(void * location, thread_id_t tid);

	void add_to_write_history(void * location, uint64_t write_val);
	HashTable<void *, value_set_t *, uintptr_t, 4> * getWriteHistory() { return &write_history; }
	void add_to_loc_func_nodes_map(void * location, FuncNode * node);

	void set_new_exec_flag();
	void print_write();
	void print_func_node();

	MEMALLOC
private:
	uint32_t func_counter;

	/* map function names to integer ids */
	HashTable<const char *, uint32_t, uintptr_t, 4, model_malloc, model_calloc, model_free> func_map;

	/* map integer ids to function names */
	ModelVector<const char *> func_map_rev;

	ModelVector<FuncNode *> func_nodes;

	HashTable<void *, value_set_t *, uintptr_t, 4> write_history;
	HashTable<void *, SnapList<FuncNode *> *, uintptr_t, 4> loc_func_nodes_map;
};

#endif	/* __HISTORY_H__ */
