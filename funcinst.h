#include "action.h"
#include "hashtable.h"

class ModelAction;

typedef ModelList<FuncInst *> func_inst_list_mt;

class FuncInst {
public:
	FuncInst(ModelAction *act);
	~FuncInst();

	//ModelAction * get_action() const { return action; }
	const char * get_position() const { return position; }
	void * get_location() const { return location; }
	action_type get_type() const { return type; }

	bool add_pred(FuncInst * other);
	bool add_succ(FuncInst * other);

	FuncInst * search_in_collision(ModelAction *act);

	func_inst_list_mt * get_collisions() { return &collisions; }
	func_inst_list_mt * get_preds() { return &predecessors; }
	func_inst_list_mt * get_succs() { return &successors; }

	MEMALLOC
private:
	//ModelAction * const action;
	const char * position;
	void *location;
	action_type type;

	func_inst_list_mt collisions;
	func_inst_list_mt predecessors;
	func_inst_list_mt successors;
};
