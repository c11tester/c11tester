#include "stl-model.h"
#include "common.h"
#include "hashtable.h"
#include "modeltypes.h"

/* forward declaration */
class ModelAction;

typedef ModelList<ModelAction *> action_mlist_t;

class ModelHistory {
public:
	ModelHistory();

	void enter_function(const uint32_t func_id, thread_id_t tid);
	void exit_function(const uint32_t func_id, thread_id_t tid);

	uint32_t get_func_counter() { return func_id; }
	void incr_func_counter() { func_id++; }

	HashTable<const char *, uint32_t, uintptr_t, 4> * getFuncMap() { return &func_map; }
	HashTable<uint32_t, action_mlist_t *, uintptr_t, 4> * getFuncHistory() { return &func_history; }

	void print();

private:
	uint32_t func_id;

	/* map function names to integer ids */
	HashTable<const char *, uint32_t, uintptr_t, 4> func_map;

	HashTable<uint32_t, action_mlist_t *, uintptr_t, 4> func_history;

	/* work_list stores a list of function ids for each thread
	 * SnapList<uint32_t> is intended to be used as a stack storing
	 * the functions that thread i has entered and yet to exit from
	 */
	HashTable<thread_id_t, SnapList<uint32_t> *, uintptr_t, 4> work_list;
};
