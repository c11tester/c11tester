#include <inttypes.h>
#include "history.h"
#include "action.h"
#include "funcnode.h"

#include "model.h"
#include "execution.h"

/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(0), /* function id starts with 0 */
	func_map(),
	func_atomics()
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();

	if ( thrd_func_list->size() <= id )
		thrd_func_list->resize( id + 1 );

	func_id_list_t * func_list = thrd_func_list->at(id);
	if (func_list == NULL) {
		func_list = new func_id_list_t();
		thrd_func_list->at(id) = func_list;
	}

	func_list->push_back(func_id);
}

void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();

	func_id_list_t * func_list = thrd_func_list->at( id_to_int(tid) );
	uint32_t last_func_id = func_list->back();

	if (last_func_id == func_id) {
		func_list->pop_back();
	} else {
		model_print("trying to exit with a wrong function id\n");
		model_print("--- last_func: %d, func_id: %d\n", last_func_id, func_id);
	}
}

void ModelHistory::add_func_atomic(ModelAction *act, thread_id_t tid)
{
	/* return if thread i has not entered any function or has exited
	   from all functions */
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();

	uint32_t id = id_to_int(tid);
	if ( thrd_func_list->size() <= id )
		return;
	else if (thrd_func_list->at(id) == NULL)
		return;

	/* get the function id that thread i is currently in */
	func_id_list_t * func_list = thrd_func_list->at(id);
	uint32_t func_id = func_list->back();

	if ( func_atomics.size() <= func_id )
		func_atomics.resize( func_id + 1 );

	FuncNode * func_node = func_atomics[func_id];
	if (func_node == NULL) {
		func_node = new FuncNode();
		func_atomics[func_id] = func_node;
	}

	func_node->add_action(act);
}

void ModelHistory::print()
{
	for (uint32_t i = 0; i < func_atomics.size(); i++ ) {
		FuncNode * funcNode = func_atomics[i];
		func_inst_list_t * inst_list = funcNode->get_inst_list();

		if (funcNode == NULL)
			continue;

		model_print("function with id: %d has following actions\n", i);
		func_inst_list_t::iterator it;
		for (it = inst_list->begin(); it != inst_list->end(); it++) {
			FuncInst *inst = *it;
			model_print("type: %d, at: %s\n", inst->get_type(), inst->get_position());
		}
	}
}
