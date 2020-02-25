#ifndef __FUNCNODE_H__
#define __FUNCNODE_H__

#include "hashset.h"
#include "hashfunction.h"
#include "classlist.h"
#include "threads-model.h"

#define MAX_DIST 10

typedef ModelList<FuncInst *> func_inst_list_mt;
typedef ModelVector<Predicate *> predicate_trace_t;

typedef HashTable<void *, FuncInst *, uintptr_t, 0, model_malloc, model_calloc, model_free> loc_inst_map_t;
typedef HashTable<FuncInst *, uint32_t, uintptr_t, 0, model_malloc, model_calloc, model_free> inst_id_map_t;
typedef HashTable<FuncInst *, Predicate *, uintptr_t, 0, model_malloc, model_calloc, model_free> inst_pred_map_t;

typedef enum edge_type {
	IN_EDGE, OUT_EDGE, BI_EDGE
} edge_type_t;

class FuncNode {
public:
	FuncNode(ModelHistory * history);
	~FuncNode();

	uint32_t get_func_id() { return func_id; }
	const char * get_func_name() { return func_name; }
	void set_func_id(uint32_t id) { func_id = id; }
	void set_func_name(const char * name) { func_name = name; }
	void set_new_exec_flag();

	void add_inst(ModelAction *act);
	FuncInst * create_new_inst(ModelAction *act);
	FuncInst * get_inst(ModelAction *act);

	func_inst_list_mt * get_inst_list() { return &inst_list; }
	func_inst_list_mt * get_entry_insts() { return &entry_insts; }
	void add_entry_inst(FuncInst * inst);

	void function_entry_handler(thread_id_t tid);
	void function_exit_handler(thread_id_t tid);

	void update_tree(ModelAction * act);

	void add_to_val_loc_map(uint64_t val, void * loc);
	void add_to_val_loc_map(value_set_t * values, void * loc);
	void update_loc_may_equal_map(void * new_loc, loc_set_t * old_locations);

	Predicate * get_predicate_tree_position(thread_id_t tid);

	void add_predicate_to_trace(thread_id_t tid, Predicate *pred);

	uint64_t get_associated_read(thread_id_t tid, FuncInst * inst);

	void add_out_edge(FuncNode * other);
	ModelList<FuncNode *> * get_out_edges() { return &out_edges; }
	int compute_distance(FuncNode * target, int max_step = MAX_DIST);

	void print_predicate_tree();

	MEMALLOC
private:
	uint32_t func_id;
	const char * func_name;
	ModelHistory * history;
	Predicate * predicate_tree_entry;	// A dummy node whose children are the real entries
	Predicate * predicate_tree_exit;	// A dummy node

	uint32_t inst_counter;
	uint32_t marker;
	uint32_t exit_count;
	ModelVector< ModelVector<uint32_t> *> thrd_markers;
	ModelVector<int> thrd_recursion_depth;	// Recursion depth starts from 0 to match with vector indexes.

	void init_marker(thread_id_t tid);

	/* Use source line number as the key of hashtable, to check if
	 * atomic operation with this line number has been added or not
	 */
	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> func_inst_map;

	/* List of all atomic actions in this function */
	func_inst_list_mt inst_list;

	/* Possible entry atomic actions in this function */
	func_inst_list_mt entry_insts;

	/* Map a FuncInst to the its predicate when updating predicate trees */
	ModelVector< ModelVector<inst_pred_map_t *> * > thrd_inst_pred_maps;

	/* Number FuncInsts to detect loops when updating predicate trees */
	ModelVector< ModelVector<inst_id_map_t *> *> thrd_inst_id_maps;

	/* Detect read actions at the same locations when updating predicate trees */
	ModelVector< ModelVector<loc_inst_map_t *> *> thrd_loc_inst_maps;

	void init_local_maps(thread_id_t tid);
	void reset_local_maps(thread_id_t tid);

	void update_inst_tree(func_inst_list_t * inst_list);
	void update_predicate_tree(ModelAction * act);
	bool follow_branch(Predicate ** curr_pred, FuncInst * next_inst, ModelAction * next_act, Predicate ** unset_predicate);

	void infer_predicates(FuncInst * next_inst, ModelAction * next_act, SnapVector<struct half_pred_expr *> * half_pred_expressions);
	void generate_predicates(Predicate * curr_pred, FuncInst * next_inst, SnapVector<struct half_pred_expr *> * half_pred_expressions);
	bool amend_predicate_expr(Predicate * curr_pred, FuncInst * next_inst, ModelAction * next_act);

	/* Set of locations read by this FuncNode */
	loc_set_t * read_locations;

	/* Set of locations written to by this FuncNode */
	loc_set_t * write_locations;

	/* Keeps track of locations that have the same values written to */
	HashTable<uint64_t, loc_set_t *, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free, int64_hash> * val_loc_map;

	/* Keeps track of locations that may share the same value as key, deduced from val_loc_map */
	HashTable<void *, loc_set_t *, uintptr_t, 0> * loc_may_equal_map;

	HashTable<FuncInst *, bool, uintptr_t, 0, model_malloc, model_calloc, model_free> likely_null_set;

	bool likely_reads_from_null(ModelAction * read);
	// value_set_t * values_may_read_from;

	/* Run-time position in the predicate tree for each thread
	 * The inner vector is used to deal with recursive functions. */
	ModelVector< ModelVector<Predicate *> * > thrd_predicate_tree_position;

	ModelVector< ModelVector<predicate_trace_t *> * > thrd_predicate_trace;

	void set_predicate_tree_position(thread_id_t tid, Predicate * pred);

	void init_predicate_tree_data_structure(thread_id_t tid);
	void reset_predicate_tree_data_structure(thread_id_t tid);

	/* Store the relation between this FuncNode and other FuncNodes */
	HashTable<FuncNode *, edge_type_t, uintptr_t, 0, model_malloc, model_calloc, model_free> edge_table;

	/* FuncNodes that follow this node */
	ModelList<FuncNode *> out_edges;

	void update_predicate_tree_weight(thread_id_t tid);
};

#endif	/* __FUNCNODE_H__ */
