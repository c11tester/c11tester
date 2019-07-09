#include "action.h"
#include "funcinst.h"
#include "hashtable.h"

class ModelAction;

typedef ModelList<FuncInst *> func_inst_list_mt;

class FuncNode {
public:
	FuncNode();
	~FuncNode();

	uint32_t get_func_id() { return func_id; }
	const char * get_func_name() { return func_name; }
	void set_func_id(uint32_t id) { func_id = id; }
	void set_func_name(const char * name) { func_name = name; }

	FuncInst * get_or_add_action(ModelAction *act);

	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncInsts() { return &func_insts; }
	func_inst_list_mt * get_inst_list() { return &inst_list; }
	func_inst_list_mt * get_entry_insts() { return &entry_insts; }
	void add_entry_inst(FuncInst * inst);

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
	func_inst_list_mt inst_list;

	/* possible entry atomic instructions in this function */
	func_inst_list_mt entry_insts;
};
