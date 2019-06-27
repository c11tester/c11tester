#include "action.h"
#include "hashtable.h"

class ModelAction;

typedef ModelList<const ModelAction *> action_mlist_t;
typedef SnapList<uint32_t> func_id_list_t;

class FuncInst {
public: 
	FuncInst(ModelAction *act);
	~FuncInst();

	ModelAction * get_action() const { return action; }
	const char * get_position() const { return position; }
private:
	ModelAction * const action;
	const char * position;
};

class FuncNode {
public:
	FuncNode();
	~FuncNode();

	HashTable<const char *, FuncInst *, uintptr_t, 4> * getFuncInsts() { return &func_insts; }
private:
	HashTable<const char *, FuncInst *, uintptr_t, 4> func_insts;
};

