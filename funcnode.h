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

	func_inst_list_t * get_collisions() { return &collisions; }

	FuncInst * search_in_collision(ModelAction *act) {
		action_type type = act->get_type();

		func_inst_list_t::iterator it;
		for (it = collisions.begin(); it != collisions.end(); it++) {
			FuncInst * inst = *it;
			if ( inst->get_type() == type )
				return inst;
		}
		return NULL;
	}

	MEMALLOC
private:
	//ModelAction * const action;
	const char * position;
	void *location;
	action_type type;

	func_inst_list_t collisions;
};

class FuncNode {
public:
	FuncNode();
	~FuncNode();

	void add_action(ModelAction *act);

	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncInsts() { return &func_insts; }
	func_inst_list_t * get_inst_list() { return &inst_list; }

	uint32_t get_func_id() { return func_id; }
	const char * get_func_name() { return func_name; }
	void set_func_id(uint32_t id) { func_id = id; }
	void set_func_name(const char * name) { func_name = name; }

	MEMALLOC
private:
	uint32_t func_id;
	const char * func_name;

	/* Use source line number as the key of hashtable, to check if 
	 * atomic operation with this line number has been added or not
	 *
	 * To do: cds_atomic_compare_exchange contains three atomic operations
	 * that are feeded with the same source line number by llvm pass
	 */
	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> func_insts;

	/* list of all atomic instructions in this function */
	func_inst_list_t inst_list;

	/* possible entry (atomic) instructions in this function */
	func_inst_list_t entry_insts;
};
