#include "stl-model.h"
#include "common.h"
#include "hashtable.h"
#include "threads-model.h"

typedef SnapList<uint32_t> func_id_list_t;

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
	ModelVector<FuncNode *> * getFuncAtomics() { return &func_atomics; }

	void print();

	MEMALLOC
private:
	uint32_t func_counter;

	/* map function names to integer ids */ 
	HashTable<const char *, uint32_t, uintptr_t, 4> func_map;

	ModelVector<FuncNode *> func_atomics;

	/* Work_list stores a list of function ids for each thread. 
	 * Each element in work_list is intended to be used as a stack storing
	 * the functions that thread i has entered and yet to exit from 
	 */

	/* todo: move work_list to execution.cc to avoid seg fault */
	SnapVector< func_id_list_t * > work_list;
};
