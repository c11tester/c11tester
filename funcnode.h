#include "action.h"
#include "hashtable.h"

class ModelAction;

typedef ModelList<FuncInst *> func_inst_list_t;

class FuncInst {
public:
	FuncInst(ModelAction *act);
	~FuncInst();

	//ModelAction * get_action() const { return action; }
	const char * get_position() const { return position; }
	void * get_location() const { return location; }
	action_type get_type() const { return type; }

	MEMALLOC
private:
	//ModelAction * const action;
	const char * position;
	void *location;
	action_type type;
};

class FuncNode {
public:
	FuncNode();
	~FuncNode();

	void add_action(ModelAction *act);

	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncInsts() { return &func_insts; }
	func_inst_list_t * get_inst_list() { return &inst_list; }

	MEMALLOC
private:
	/* Use source line number as the key of hashtable
	 *
	 * To do: cds_atomic_compare_exchange contains three atomic operations
	 * that are feeded with the same source line number by llvm pass
	 */
	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> func_insts;

	func_inst_list_t inst_list;
};
