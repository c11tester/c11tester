#ifndef __FUNCNODE_H__
#define __FUNCNODE_H__

#include "action.h"
#include "funcinst.h"
#include "hashtable.h"
#include "hashset.h"
#include "predicate.h"

typedef ModelList<FuncInst *> func_inst_list_mt;
typedef HashTable<void *, uint64_t, uintptr_t, 4, model_malloc, model_calloc, model_free> read_map_t;

class FuncNode {
public:
	FuncNode();
	~FuncNode();

	uint32_t get_func_id() { return func_id; }
	const char * get_func_name() { return func_name; }
	void set_func_id(uint32_t id) { func_id = id; }
	void set_func_name(const char * name) { func_name = name; }

	void add_inst(ModelAction *act);
	FuncInst * get_inst(ModelAction *act);

	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncInstMap() { return &func_inst_map; }
	func_inst_list_mt * get_inst_list() { return &inst_list; }
	func_inst_list_mt * get_entry_insts() { return &entry_insts; }
	void add_entry_inst(FuncInst * inst);

	void update_tree(action_list_t * act_list);
	void update_inst_tree(func_inst_list_t * inst_list);

	void store_read(ModelAction * act, uint32_t tid);
	uint64_t query_last_read(void * location, uint32_t tid);
	void clear_read_map(uint32_t tid);

	/* TODO: generate EQUALITY or NULLITY predicate based on write_history in history.cc */
	void update_predicate_tree(action_list_t * act_list, HashTable<ModelAction *, FuncInst *, uintptr_t, 4> * act_inst_map);
	bool follow_branch(Predicate ** curr_pred, FuncInst * next_inst, HashTable<FuncInst *, uint64_t, uintptr_t, 4> * read_val_map, HashTable<void *, FuncInst *, uintptr_t, 4>* loc_inst_map);
	void print_predicate_tree();

	void print_last_read(uint32_t tid);

	MEMALLOC
private:
	uint32_t func_id;
	const char * func_name;
	bool predicate_tree_initialized;
	Predicate * predicate_tree_entry;	// a dummy node whose children are the real entries

	/* Use source line number as the key of hashtable, to check if
	 * atomic operation with this line number has been added or not
	 */
	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> func_inst_map;

	/* list of all atomic actions in this function */
	func_inst_list_mt inst_list;

	/* possible entry atomic actions in this function */
	func_inst_list_mt entry_insts;

	/* Store the values read by atomic read actions per memory location for each thread */
	ModelVector<read_map_t *> thrd_read_map;
	HashTable<FuncInst *, Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> predicate_tree_backedges;
};

#endif /* __FUNCNODE_H__ */
