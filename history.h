#include "stl-model.h"
#include "common.h"
#include "hashtable.h"
#include "threads-model.h"

/* forward declaration */
class ModelAction;

typedef ModelList<const ModelAction *> action_mlist_t;
typedef SnapList<uint32_t> func_id_list_t;

class HistoryNode {
public: 
	HistoryNode(ModelAction *act);
	~HistoryNode();

	ModelAction * get_action() const { return action; }
	const char * get_position() const { return position; }
private:
	ModelAction * const action;
	const char * position;
};

class ModelHistory {
public:
	ModelHistory();
	~ModelHistory();

	void enter_function(const uint32_t func_id, thread_id_t tid);
	void exit_function(const uint32_t func_id, thread_id_t tid);

        uint32_t get_func_counter() { return func_counter; }
        void incr_func_counter() { func_counter++; }

	void add_func_atomic(ModelAction *act, thread_id_t tid);

        HashTable<const char *, uint32_t, uintptr_t, 4> * getFuncMap() { return &func_map; }
	ModelVector< action_mlist_t * > * getFuncAtomics() { return &func_atomics; }

	void print();
private:
	uint32_t func_counter;

        /* map function names to integer ids */ 
        HashTable<const char *, uint32_t, uintptr_t, 4> func_map;

	ModelVector< action_mlist_t * > func_atomics;

	/* Work_list stores a list of function ids for each thread. 
	 * Each element in work_list is intended to be used as a stack storing
	 * the functions that thread i has entered and yet to exit from 
	 */
	SnapVector< func_id_list_t * > work_list;
};
