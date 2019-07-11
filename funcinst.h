#include "action.h"
#include "hashtable.h"

class ModelAction;

typedef ModelList<FuncInst *> func_inst_list_mt;

class FuncInst {
public:
	FuncInst(ModelAction *act, FuncNode *func_node);
	~FuncInst();

	//ModelAction * get_action() const { return action; }
	const char * get_position() const { return position; }
	void * get_location() const { return location; }
	action_type get_type() const { return type; }
	FuncNode * get_func_node() const { return func_node; }

	bool add_pred(FuncInst * other);
	bool add_succ(FuncInst * other);

	FuncInst * search_in_collision(ModelAction *act);

	func_inst_list_mt * get_collisions() { return &collisions; }
	func_inst_list_mt * get_preds() { return &predecessors; }
	func_inst_list_mt * get_succs() { return &successors; }

	bool is_read() const;

	MEMALLOC
private:
	//ModelAction * const action;
	const char * position;
	void * location;
	action_type type;
	FuncNode * func_node;

	/* collisions store a list of FuncInsts with the same position
	 * but different action types. For example, CAS is broken down
	 * as three different atomic operations in cmodelint.cc */
	func_inst_list_mt collisions;

	func_inst_list_mt predecessors;
	func_inst_list_mt successors;
};
