#include <inttypes.h>
#include "history.h"
#include "action.h"
#include "funcnode.h"
#include "common.h"

#include "model.h"
#include "execution.h"


/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(1), /* function id starts with 1 */
	func_map(),
	func_map_rev(),
	func_nodes()
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	//model_print("thread %d entering func %d\n", tid, func_id);
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
			thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	if ( thrd_func_list->size() <= id ) {
		thrd_func_list->resize( id + 1 );
		thrd_func_inst_lists->resize( id + 1 );
	}

	func_id_list_t * func_list = thrd_func_list->at(id);
	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);

	if (func_list == NULL) {
		func_list = new func_id_list_t();
		thrd_func_list->at(id) = func_list;
	}

	if (func_inst_lists == NULL) {
		func_inst_lists = new SnapList< func_inst_list_t *>();
		thrd_func_inst_lists->at(id) = func_inst_lists;
	}

	func_list->push_back(func_id);
	func_inst_lists->push_back( new func_inst_list_t() );
}

/* @param func_id a non-zero value */
void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
			thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	func_id_list_t * func_list = thrd_func_list->at(id);
	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);

	uint32_t last_func_id = func_list->back();

	if (last_func_id == func_id) {
		func_list->pop_back();

		/* clear read map upon exiting functions */
		FuncNode * func_node = func_nodes[func_id];
		func_node->clear_read_map(tid);

		func_inst_list_t * curr_inst_list = func_inst_lists->back();
		func_node->link_insts(curr_inst_list);

		func_inst_lists->pop_back();
	} else {
		model_print("trying to exit with a wrong function id\n");
		model_print("--- last_func: %d, func_id: %d\n", last_func_id, func_id);
	}
	//model_print("thread %d exiting func %d\n", tid, func_id);
}

void ModelHistory::process_action(ModelAction *act, thread_id_t tid)
{
	/* return if thread i has not entered any function or has exited
	   from all functions */
	SnapVector<func_id_list_t *> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
			thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	uint32_t id = id_to_int(tid);
	if ( thrd_func_list->size() <= id )
		return;
	else if (thrd_func_list->at(id) == NULL)
		return;

	/* get the function id that thread i is currently in */
	func_id_list_t * func_list = thrd_func_list->at(id);
	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);

	uint32_t func_id = func_list->back();

	if ( func_nodes.size() <= func_id )
		func_nodes.resize( func_id + 1 );

	FuncNode * func_node = func_nodes[func_id];
	if (func_node == NULL) {
		const char * func_name = func_map_rev[func_id];
		func_node = new FuncNode();
		func_node->set_func_id(func_id);
		func_node->set_func_name(func_name);

		func_nodes[func_id] = func_node;
	}

	/* add corresponding FuncInst to func_node */
	FuncInst * inst = func_node->get_or_add_action(act);

	if (inst == NULL)
		return;

	if (inst->is_read())
		func_node->store_read(act, tid);

	/* add to curr_inst_list */
	func_inst_list_t * curr_inst_list = func_inst_lists->back();
	ASSERT(curr_inst_list != NULL);
	curr_inst_list->push_back(inst);
}

void ModelHistory::print()
{
	/* function id starts with 1 */
	for (uint32_t i = 1; i < func_nodes.size(); i++) {
		FuncNode * func_node = func_nodes[i];

		func_inst_list_mt * entry_insts = func_node->get_entry_insts();
		model_print("function %s has entry actions\n", func_node->get_func_name());

		func_inst_list_mt::iterator it;
		for (it = entry_insts->begin(); it != entry_insts->end(); it++) {
			FuncInst *inst = *it;
			model_print("type: %d, at: %s\n", inst->get_type(), inst->get_position());
		}

/*
		func_inst_list_mt * inst_list = funcNode->get_inst_list();

		model_print("function %s has following actions\n", funcNode->get_func_name());
		func_inst_list_mt::iterator it;
		for (it = inst_list->begin(); it != inst_list->end(); it++) {
			FuncInst *inst = *it;
			model_print("type: %d, at: %s\n", inst->get_type(), inst->get_position());
		}
*/
	}
}
