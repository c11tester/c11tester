#include <inttypes.h>
#include "history.h"
#include "action.h"


/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(0), /* function id starts with 0 */
	func_map(),
	func_atomics(),
	work_list(2)	/* we have at least two threads */
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
//	model_print("entering function: %d\n", func_id);

	uint32_t id = id_to_int(tid);
	if ( work_list.size() <= id )
		work_list.resize( id + 1 );

	func_id_list_t * func_list = work_list[id];
	if (func_list == NULL) {
		func_list = new func_id_list_t();
		work_list[id] = func_list;
	}

	func_list->push_back(func_id);
}

void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	func_id_list_t * func_list = work_list[ id_to_int(tid) ];
	uint32_t last_func_id = func_list->back();

	if (last_func_id == func_id) {
		func_list->pop_back();
	} else {
		model_print("trying to exit with a wrong function id\n");
		model_print("--- last_func: %d, func_id: %d\n", last_func_id, func_id);
	}

//	model_print("exiting function: %d\n", func_id);
}

void ModelHistory::add_func_atomic(ModelAction *act, thread_id_t tid) {
	/* return if thread i has not entered any function or has exited
	   from all functions */
	uint32_t id = id_to_int(tid);
	if ( work_list.size() <= id )
		return;
	else if (work_list[id] == NULL)
		return;

	/* get the function id that thread i is currently in */
	func_id_list_t * func_list = work_list[id];
	uint32_t func_id = func_list->back();

	if ( func_atomics.size() <= func_id )
		func_atomics.resize( func_id + 1 );

	action_mlist_t * atomic_list = func_atomics[func_id];
	if (atomic_list == NULL) {
		atomic_list = new action_mlist_t();
		func_atomics[func_id] = atomic_list;
	}

	atomic_list->push_back(act);

	model_print("func id: %d, added atomic acts: %d\n", func_id, act->get_type());
}

void ModelHistory::print() {
	for (uint32_t i = 0; i < func_atomics.size(); i++ ) {
		action_mlist_t * atomic_list = func_atomics[i];

		if (atomic_list == NULL)
			continue;

		model_print("function with id: %d has following actions\n", i);
		action_mlist_t::iterator it;
		for (it = atomic_list->begin(); it != atomic_list->end(); it++) {
			const ModelAction *act = *it;
			act->print();
		}
	}
}
