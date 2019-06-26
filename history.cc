#include <inttypes.h>
#include "history.h"
#include "action.h"

ModelHistory::ModelHistory() :
	func_id(1),	/* function id starts with 1 */
	func_map(),
	func_history(),
	work_list()
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	if ( !work_list.contains(tid) ) {
		// This thread has not been pushed to work_list
		SnapList<uint32_t> * func_list = new SnapList<uint32_t>();
		func_list->push_back(func_id);
		work_list.put(tid, func_list);
	} else {
		SnapList<uint32_t> * func_list = work_list.get(tid);
		func_list->push_back(func_id);
	}
}

void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	SnapList<uint32_t> * func_list = work_list.get(tid);
	uint32_t last_func_id = func_list->back();

	if (last_func_id == func_id) {
		func_list->pop_back();
	} else {
		model_print("trying to exit with a wrong function id\n");
		model_print("--- last_func: %d, func_id: %d\n", last_func_id, func_id);
	}
}
