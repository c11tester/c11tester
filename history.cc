#include <inttypes.h>
#include "history.h"
#include "action.h"
#include "funcnode.h"
#include "common.h"

#include "model.h"
#include "execution.h"
#include "newfuzzer.h"

/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(1),	/* function id starts with 1 */
	func_map(),
	func_map_rev(),
	func_nodes(),
	write_history(),		// snapshot data structure
	loc_func_nodes_map(),		// shapshot data structure
	thrd_last_entered_func()	// snapshot data structure
{}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	//model_print("thread %d entering func %d\n", tid, func_id);
	uint id = id_to_int(tid);
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<action_list_t *> *> *
		thrd_func_act_lists = model->get_execution()->get_thrd_func_act_lists();

	if ( thrd_func_list->size() <= id ) {
		uint oldsize = thrd_func_list->size();
		thrd_func_list->resize( id + 1 );
		thrd_func_act_lists->resize( id + 1 );

		for (uint i = oldsize; i < id + 1; i++) {
			new (&(*thrd_func_list)[i]) func_id_list_t();
			// push 0 as a dummy function id to a void seg fault
			(*thrd_func_list)[i].push_back(0);

			(*thrd_func_act_lists)[i] = new SnapList<action_list_t *>();
		}
	}

	while (	thrd_last_entered_func.size() <= id ) {
		thrd_last_entered_func.push_back(0);	// 0 is a dummy function id
	}

	SnapList<action_list_t *> * func_act_lists = (*thrd_func_act_lists)[id];
	func_act_lists->push_back( new action_list_t() );

	uint32_t last_entered_func_id = thrd_last_entered_func[id];
	thrd_last_entered_func[id] = func_id;
	(*thrd_func_list)[id].push_back(func_id);

	if ( func_nodes.size() <= func_id )
		resize_func_nodes( func_id + 1 );

	FuncNode * func_node = func_nodes[func_id];
	func_node->init_predicate_tree_position(tid);
	func_node->init_inst_act_map(tid);

	/* Add edges between FuncNodes */
	if (last_entered_func_id != 0) {
		FuncNode * last_func_node = func_nodes[last_entered_func_id];
		add_edges_between(last_func_node, func_node);
	}
}

/* @param func_id a non-zero value */
void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	uint32_t id = id_to_int(tid);
	SnapVector<func_id_list_t> * thrd_func_list = model->get_execution()->get_thrd_func_list();
	SnapVector< SnapList<action_list_t *> *> *
		thrd_func_act_lists = model->get_execution()->get_thrd_func_act_lists();

	SnapList<action_list_t *> * func_act_lists = (*thrd_func_act_lists)[id];
	uint32_t last_func_id = (*thrd_func_list)[id].back();

	if (last_func_id == func_id) {
		FuncNode * func_node = func_nodes[func_id];
		func_node->set_predicate_tree_position(tid, NULL);
		func_node->reset_inst_act_map(tid);

		action_list_t * curr_act_list = func_act_lists->back();

		/* defer the processing of curr_act_list until the function has exits a few times 
		 * (currently twice) so that more information can be gathered to infer nullity predicates.
		 */
		func_node->incr_exit_count();
		if (func_node->get_exit_count() >= 2) {
			SnapList<action_list_t *> * action_list_buffer = func_node->get_action_list_buffer();
			while (action_list_buffer->size() > 0) {
				action_list_t * act_list = action_list_buffer->back();
				action_list_buffer->pop_back();
				func_node->update_tree(act_list);
			}

			func_node->update_tree(curr_act_list);
		} else
			func_node->get_action_list_buffer()->push_front(curr_act_list);

		(*thrd_func_list)[id].pop_back();
		func_act_lists->pop_back();
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

	for (uint32_t id = old_size; id < new_size; id++) {
		const char * func_name = func_map_rev[id];
		FuncNode * func_node = new FuncNode(this);
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
	SnapVector< SnapList<action_list_t *> *> *
		thrd_func_act_lists = model->get_execution()->get_thrd_func_act_lists();

	uint32_t id = id_to_int(tid);
	if ( thrd_func_list->size() <= id )
		return;

	/* get the function id that thread i is currently in */
	uint32_t func_id = (*thrd_func_list)[id].back();
	SnapList<action_list_t *> * func_act_lists = (*thrd_func_act_lists)[id];

	if (act->is_write()) {
		void * location = act->get_location();
		uint64_t value = act->get_write_value();
		update_write_history(location, value);

		/* Update FuncNodes that may read from this location */
		SnapList<FuncNode *> * func_nodes = loc_func_nodes_map.get(location);
		if (func_nodes != NULL) {
			sllnode<FuncNode *> * it = func_nodes->begin();
			for (; it != NULL; it = it->getNext()) {
				FuncNode * func_node = it->getVal();
				func_node->add_to_val_loc_map(value, location);
			}
		}
	}

	/* the following does not care about actions without a position */
	if (func_id == 0 || act->get_position() == NULL)
		return;

	bool second_part_of_rmw = act->is_rmwc() || act->is_rmw();

	action_list_t * curr_act_list = func_act_lists->back();
	ASSERT(curr_act_list != NULL);

	modelclock_t curr_seq_number = act->get_seq_number();
	/* Skip actions that are second part of a read modify write or actions with the same sequence number */
	if (curr_act_list->size() != 0) {
		ModelAction * last_act = curr_act_list->back();
		if (second_part_of_rmw || last_act->get_seq_number() == curr_seq_number)
			return;
	}

	/* skip actions that are paused by fuzzer (sequence number is 0) */
	if (curr_seq_number == 0)
		return;

	FuncNode * func_node = func_nodes[func_id];

	/* add to curr_inst_list */
	curr_act_list->push_back(act);
	func_node->add_inst(act);

	if (act->is_read()) {
		func_node->update_inst_act_map(tid, act);

		// Update predicate tree position
		Fuzzer * fuzzer = model->get_execution()->getFuzzer();
		Predicate * selected_branch = fuzzer->get_selected_child_branch(tid);
		func_node->set_predicate_tree_position(tid, selected_branch);
	}
}

/* return the FuncNode given its func_id  */
FuncNode * ModelHistory::get_func_node(uint32_t func_id)
{
	if (func_nodes.size() <= func_id)	// this node has not been added to func_nodes
		return NULL;

	return func_nodes[func_id];
}

void ModelHistory::update_write_history(void * location, uint64_t write_val)
{
	value_set_t * write_set = write_history.get(location);

	if (write_set == NULL) {
		write_set = new value_set_t();
		write_history.put(location, write_set);
	}

	write_set->add(write_val);
}

void ModelHistory::update_loc_func_nodes_map(void * location, FuncNode * node)
{
	SnapList<FuncNode *> * func_node_list = loc_func_nodes_map.get(location);
	if (func_node_list == NULL) {
		func_node_list = new SnapList<FuncNode *>();
		loc_func_nodes_map.put(location, func_node_list);
	}

	func_node_list->push_back(node);
}

void ModelHistory::update_loc_wr_func_nodes_map(void * location, FuncNode * node)
{
	SnapList<FuncNode *> * func_node_list = loc_wr_func_nodes_map.get(location);
	if (func_node_list == NULL) {
		func_node_list = new SnapList<FuncNode *>();
		loc_func_nodes_map.put(location, func_node_list);
	}

	func_node_list->push_back(node);
}

/* Reallocate some snapshotted memories when new executions start */
void ModelHistory::set_new_exec_flag()
{
	for (uint i = 1; i < func_nodes.size(); i++) {
		FuncNode * func_node = func_nodes[i];
		func_node->set_new_exec_flag();
	}
}

/* Add edges between FuncNodes */
void ModelHistory::add_edges_between(FuncNode * prev_node, FuncNode * next_node)
{
	prev_node->add_out_edge(next_node);
}

void ModelHistory::dump_func_node_graph()
{
	model_print("digraph func_node_graph {\n");
	for (uint i = 1; i < func_nodes.size(); i++) {
		FuncNode * node = func_nodes[i];
		ModelList<FuncNode *> * out_edges = node->get_out_edges();

		model_print("\"%p\" [label=\"%s\"]\n", node, node->get_func_name());
		mllnode<FuncNode *> * it;
		for (it = out_edges->begin(); it != NULL; it = it->getNext()) {
			FuncNode * other = it->getVal();
			model_print("\"%p\" -> \"%p\"\n", node, other);
		}
	}
	model_print("}\n");
}

void ModelHistory::print_func_node()
{
	/* function id starts with 1 */
	for (uint32_t i = 1; i < func_nodes.size(); i++) {
		FuncNode * func_node = func_nodes[i];

		func_inst_list_mt * entry_insts = func_node->get_entry_insts();
		model_print("function %s has entry actions\n", func_node->get_func_name());

		mllnode<FuncInst*>* it;
		for (it = entry_insts->begin();it != NULL;it=it->getNext()) {
			FuncInst *inst = it->getVal();
			model_print("type: %d, at: %s\n", inst->get_type(), inst->get_position());
		}
	}
}
