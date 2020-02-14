#ifndef __FUNCINST_H__
#define __FUNCINST_H__

#include "action.h"
#include "classlist.h"
#include "hashtable.h"
#include "threads-model.h"

typedef ModelList<FuncInst *> func_inst_list_mt;

class FuncInst {
public:
	FuncInst(ModelAction *act, FuncNode *func_node);
	~FuncInst();

	const char * get_position() const { return position; }

	void * get_location() const { return location; }
	void set_location(void * loc) { location = loc; }

	action_type get_type() const { return type; }
	memory_order get_mo() const { return order; }
	FuncNode * get_func_node() const { return func_node; }

	bool add_pred(FuncInst * other);
	bool add_succ(FuncInst * other);

	FuncInst * search_in_collision(ModelAction *act);
	void add_to_collision(FuncInst * inst);

	func_inst_list_mt * get_preds() { return &predecessors; }
	func_inst_list_mt * get_succs() { return &successors; }

	bool is_read() const;
	bool is_write() const;
	bool is_single_location() { return single_location; }
	void not_single_location() { single_location = false; }

	void set_execution_number(int new_number) { execution_number = new_number; }
	int get_execution_number() { return execution_number; }

	void set_associated_read(thread_id_t tid, int index, uint32_t marker, uint64_t read_val);
	uint64_t get_associated_read(thread_id_t tid, int index, uint32_t marker);

	void print();

	MEMALLOC
private:
	const char * position;

	/* Atomic operations with the same source line number may act at different
	 * memory locations, such as the next field of the head pointer in ms-queue.
	 * location only stores the memory location when this FuncInst was constructed.
	 */
	void * location;

	/* NOTE: for rmw actions, func_inst and act may have different
	 * action types because of action type conversion in ModelExecution */
	action_type type;

	memory_order order;
	FuncNode * func_node;

	bool single_location;
	int execution_number;

	ModelVector< ModelVector<uint64_t> * > associated_reads;
	ModelVector< ModelVector<uint32_t> * > thrd_markers;

	/**
	 * Collisions store a list of FuncInsts with the same position
	 * but different action types. For example,
	 * <code>volatile int x; x++;</code> produces read and write
	 * actions with the same position.
	 */
	func_inst_list_mt collisions;

	func_inst_list_mt predecessors;
	func_inst_list_mt successors;
};

#endif	/* __FUNCINST_H__ */
