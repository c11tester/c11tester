#include <inttypes.h>
#include "history.h"
#include "action.h"
#include "funcnode.h"
#include "common.h"

#include "model.h"
#include "execution.h"


/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(1),	/* function id starts with 1 */
	func_map(),
	func_map_rev(),
	func_nodes(),
	write_history()
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	//model_print("thread %d entering func %d\n", tid, func_id);
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
		thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	if ( thrd_func_list->size() <= id ) {
		thrd_func_list->resize( id + 1 );
		thrd_func_inst_lists->resize( id + 1 );
	}

	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);

	if (func_inst_lists == NULL) {
		func_inst_lists = new SnapList< func_inst_list_t *>();
		thrd_func_inst_lists->at(id) = func_inst_lists;
	}

	(*thrd_func_list)[id].push_back(func_id);
	func_inst_lists->push_back( new func_inst_list_t() );

	if ( func_nodes.size() <= func_id )
		resize_func_nodes( func_id + 1 );
}

/* @param func_id a non-zero value */
void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
		thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);
	uint32_t last_func_id = (*thrd_func_list)[id].back();

	if (last_func_id == func_id) {
		FuncNode * func_node = func_nodes[func_id];
		func_node->clear_read_map(tid);

		func_inst_list_t * curr_inst_list = func_inst_lists->back();
		func_node->link_insts(curr_inst_list);

		(*thrd_func_list)[id].pop_back();
		func_inst_lists->pop_back();
	} else {
		model_print("trying to exit with a wrong function id\n");
		model_print("--- last_func: %d, func_id: %d\n", last_func_id, func_id);
	}
	//model_print("thread %d exiting func %d\n", tid, func_id);
}

void ModelHistory::resize_func_nodes(uint32_t new_size)
{
	uint32_t old_size = func_nodes.size();

	if ( old_size < new_size )
		func_nodes.resize(new_size);

	for (uint32_t id = old_size;id < new_size;id++) {
		const char * func_name = func_map_rev[id];
		FuncNode * func_node = new FuncNode();
		func_node->set_func_id(id);
		func_node->set_func_name(func_name);
		func_nodes[id] = func_node;
	}
}

void ModelHistory::process_action(ModelAction *act, thread_id_t tid)
{
	/* return if thread i has not entered any function or has exited
	   from all functions */
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<func_inst_list_t *> *> *
		thrd_func_inst_lists = model->get_execution()->get_thrd_func_inst_lists();

	uint32_t id = id_to_int(tid);
	if ( thrd_func_list->size() <= id )
		return;

	/* get the function id that thread i is currently in */
	uint32_t func_id = (*thrd_func_list)[id].back();
	SnapList<func_inst_list_t *> * func_inst_lists = thrd_func_inst_lists->at(id);

	if ( func_nodes.size() <= func_id )
		resize_func_nodes( func_id + 1 );

	FuncNode * func_node = func_nodes[func_id];
	ASSERT (func_node != NULL);

	/* add corresponding FuncInst to func_node */
	FuncInst * inst = func_node->get_or_add_action(act);

	if (inst == NULL)
		return;

	//	if (inst->is_read())
	//	func_node->store_read(act, tid);

	if (inst->is_write())
		add_to_write_history(act->get_location(), act->get_write_value());

	/* add to curr_inst_list */
	func_inst_list_t * curr_inst_list = func_inst_lists->back();
	ASSERT(curr_inst_list != NULL);
	curr_inst_list->push_back(inst);
}

/* return the FuncNode given its func_id  */
FuncNode * ModelHistory::get_func_node(uint32_t func_id)
{
	if (func_nodes.size() <= func_id)	// this node has not been added
		return NULL;

	return func_nodes[func_id];
}

uint64_t ModelHistory::query_last_read(void * location, thread_id_t tid)
{
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	uint32_t id = id_to_int(tid);

	ASSERT( thrd_func_list->size() > id );
	uint32_t func_id = (*thrd_func_list)[id].back();
	FuncNode * func_node = func_nodes[func_id];

	uint64_t last_read_val = 0xdeadbeef;
	if (func_node != NULL) {
		last_read_val = func_node->query_last_read(location, tid);
	}

	return last_read_val;
}

void ModelHistory::add_to_write_history(void * location, uint64_t write_val)
{
	if ( !write_history.contains(location) )
		write_history.put(location, new write_set_t() );

	write_set_t * write_set = write_history.get(location);
	write_set->add(write_val);
}

void ModelHistory::print()
{
	/* function id starts with 1 */
	for (uint32_t i = 1;i < func_nodes.size();i++) {
		FuncNode * func_node = func_nodes[i];

		func_inst_list_mt * entry_insts = func_node->get_entry_insts();
		model_print("function %s has entry actions\n", func_node->get_func_name());

		mllnode<FuncInst*>* it;
		for (it = entry_insts->begin();it != NULL;it=it->getNext()) {
			FuncInst *inst = it->getVal();
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
