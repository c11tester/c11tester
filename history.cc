#include <inttypes.h>
#include "history.h"
#include "action.h"
#include "funcnode.h"
#include "funcinst.h"
#include "common.h"
#include "concretepredicate.h"
#include "waitobj.h"

#include "model.h"
#include "execution.h"
#include "newfuzzer.h"

/** @brief Constructor */
ModelHistory::ModelHistory() :
	func_counter(1),	/* function id starts with 1 */
	last_seq_number(INIT_SEQ_NUMBER),
	func_map(),
	func_map_rev(),
	func_nodes()
{
	/* The following are snapshot data structures */
	write_history = new HashTable<void *, value_set_t *, uintptr_t, 0>();
	loc_rd_func_nodes_map = new HashTable<void *, SnapVector<FuncNode *> *, uintptr_t, 0>();
	loc_wr_func_nodes_map = new HashTable<void *, SnapVector<FuncNode *> *, uintptr_t, 0>();
	loc_waiting_writes_map = new HashTable<void *, SnapVector<ConcretePredicate *> *, uintptr_t, 0>();
	thrd_func_list = new SnapVector<func_id_list_t>();
	thrd_last_entered_func = new SnapVector<uint32_t>();
	thrd_waiting_write = new SnapVector<ConcretePredicate *>();
	thrd_wait_obj = new SnapVector<WaitObj *>();
}

ModelHistory::~ModelHistory()
{
	// TODO: complete deconstructor; maybe not needed
	for (uint i = 0;i < thrd_wait_obj->size();i++)
		delete (*thrd_wait_obj)[i];
}

void ModelHistory::enter_function(const uint32_t func_id, thread_id_t tid)
{
	//model_print("thread %d entering func %d\n", tid, func_id);
	uint id = id_to_int(tid);

	if ( thrd_func_list->size() <= id ) {
		uint oldsize = thrd_func_list->size();
		thrd_func_list->resize( id + 1 );

		for (uint i = oldsize;i < id + 1;i++) {
			// push 0 as a dummy function id to a void seg fault
			new (&(*thrd_func_list)[i]) func_id_list_t();
			(*thrd_func_list)[i].push_back(0);
			thrd_last_entered_func->push_back(0);
		}
	}

	uint32_t last_entered_func_id = (*thrd_last_entered_func)[id];
	(*thrd_last_entered_func)[id] = func_id;
	(*thrd_func_list)[id].push_back(func_id);

	if ( func_nodes.size() <= func_id )
		resize_func_nodes( func_id + 1 );

	FuncNode * func_node = func_nodes[func_id];
	func_node->function_entry_handler(tid);

	/* Add edges between FuncNodes */
	if (last_entered_func_id != 0) {
		FuncNode * last_func_node = func_nodes[last_entered_func_id];
		last_func_node->add_out_edge(func_node);
	}

	/* Monitor the statuses of threads waiting for tid */
	// monitor_waiting_thread(func_id, tid);
}

/* @param func_id a non-zero value */
void ModelHistory::exit_function(const uint32_t func_id, thread_id_t tid)
{
	uint32_t id = id_to_int(tid);
	uint32_t last_func_id = (*thrd_func_list)[id].back();

	if (last_func_id == func_id) {
		FuncNode * func_node = func_nodes[func_id];
		func_node->function_exit_handler(tid);

		(*thrd_func_list)[id].pop_back();
	} else {
		ASSERT(false);
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
		FuncNode * func_node = new FuncNode(this);
		func_node->set_func_id(id);
		func_node->set_func_name(func_name);
		func_nodes[id] = func_node;
	}
}

void ModelHistory::process_action(ModelAction *act, thread_id_t tid)
{
	uint32_t thread_id = id_to_int(tid);
	/* Return if thread tid has not entered any function that contains atomics */
	if ( thrd_func_list->size() <= thread_id )
		return;

	/* Monitor the statuses of threads waiting for tid */
	// monitor_waiting_thread_counter(tid);

	/* Every write action should be processed, including
	 * nonatomic writes (which have no position) */
	if (act->is_write()) {
		void * location = act->get_location();
		uint64_t value = act->get_write_value();
		update_write_history(location, value);

		/* Notify FuncNodes that may read from this location */
		SnapVector<FuncNode *> * func_node_list = getRdFuncNodes(location);
		for (uint i = 0;i < func_node_list->size();i++) {
			FuncNode * func_node = (*func_node_list)[i];
			func_node->add_to_val_loc_map(value, location);
		}

		// check_waiting_write(act);
	}

	uint32_t func_id = (*thrd_func_list)[thread_id].back();

	/* The following does not care about actions that are not inside
	 * any function that contains atomics or actions without a position */
	if (func_id == 0 || act->get_position() == NULL)
		return;

	if (skip_action(act))
		return;

	FuncNode * func_node = func_nodes[func_id];
	func_node->add_inst(act);

	func_node->update_tree(act);
	last_seq_number = act->get_seq_number();
}

/* Return the FuncNode given its func_id  */
FuncNode * ModelHistory::get_func_node(uint32_t func_id)
{
	if (func_id == 0)
		return NULL;

	// This node has not been added to func_nodes
	if (func_nodes.size() <= func_id)
		return NULL;

	return func_nodes[func_id];
}

/* Return the current FuncNode when given a thread id */
FuncNode * ModelHistory::get_curr_func_node(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	uint32_t func_id = (*thrd_func_list)[thread_id].back();

	if (func_id != 0) {
		return func_nodes[func_id];
	}

	return NULL;
}

void ModelHistory::update_write_history(void * location, uint64_t write_val)
{
	value_set_t * write_set = write_history->get(location);

	if (write_set == NULL) {
		write_set = new value_set_t();
		write_history->put(location, write_set);
	}

	write_set->add(write_val);
}

void ModelHistory::update_loc_rd_func_nodes_map(void * location, FuncNode * node)
{
	SnapVector<FuncNode *> * func_node_list = getRdFuncNodes(location);
	func_node_list->push_back(node);
}

void ModelHistory::update_loc_wr_func_nodes_map(void * location, FuncNode * node)
{
	SnapVector<FuncNode *> * func_node_list = getWrFuncNodes(location);
	func_node_list->push_back(node);
}

SnapVector<FuncNode *> * ModelHistory::getRdFuncNodes(void * location)
{
	SnapVector<FuncNode *> * func_node_list = loc_rd_func_nodes_map->get(location);
	if (func_node_list == NULL) {
		func_node_list = new SnapVector<FuncNode *>();
		loc_rd_func_nodes_map->put(location, func_node_list);
	}

	return func_node_list;
}

SnapVector<FuncNode *> * ModelHistory::getWrFuncNodes(void * location)
{
	SnapVector<FuncNode *> * func_node_list = loc_wr_func_nodes_map->get(location);
	if (func_node_list == NULL) {
		func_node_list = new SnapVector<FuncNode *>();
		loc_wr_func_nodes_map->put(location, func_node_list);
	}

	return func_node_list;
}

/* When a thread is paused by Fuzzer, keep track of the condition it is waiting for */
void ModelHistory::add_waiting_write(ConcretePredicate * concrete)
{
	void * location = concrete->get_location();
	SnapVector<ConcretePredicate *> * waiting_conditions = loc_waiting_writes_map->get(location);
	if (waiting_conditions == NULL) {
		waiting_conditions = new SnapVector<ConcretePredicate *>();
		loc_waiting_writes_map->put(location, waiting_conditions);
	}

	/* waiting_conditions should not have duplications */
	waiting_conditions->push_back(concrete);

	int thread_id = id_to_int(concrete->get_tid());
	if (thrd_waiting_write->size() <= (uint) thread_id) {
		thrd_waiting_write->resize(thread_id + 1);
	}

	(*thrd_waiting_write)[thread_id] = concrete;
}

void ModelHistory::remove_waiting_write(thread_id_t tid)
{
	ConcretePredicate * concrete = (*thrd_waiting_write)[ id_to_int(tid) ];
	void * location = concrete->get_location();
	SnapVector<ConcretePredicate *> * concrete_preds = loc_waiting_writes_map->get(location);

	/* Linear search should be fine because presumably not many ConcretePredicates
	 * are at the same memory location */
	for (uint i = 0;i < concrete_preds->size();i++) {
		ConcretePredicate * current = (*concrete_preds)[i];
		if (concrete == current) {
			(*concrete_preds)[i] = concrete_preds->back();
			concrete_preds->pop_back();
			break;
		}
	}

	int thread_id = id_to_int( concrete->get_tid() );
	(*thrd_waiting_write)[thread_id] = NULL;
	delete concrete;
}

/* Check if any other thread is waiting for this write action. If so, "notify" them */
void ModelHistory::check_waiting_write(ModelAction * write_act)
{
	void * location = write_act->get_location();
	uint64_t value = write_act->get_write_value();
	SnapVector<ConcretePredicate *> * concrete_preds = loc_waiting_writes_map->get(location);
	if (concrete_preds == NULL)
		return;

	uint index = 0;
	while (index < concrete_preds->size()) {
		ConcretePredicate * concrete_pred = (*concrete_preds)[index];
		SnapVector<struct concrete_pred_expr> * concrete_exprs = concrete_pred->getExpressions();
		bool satisfy_predicate = true;
		/* Check if the written value satisfies every predicate expression */
		for (uint i = 0;i < concrete_exprs->size();i++) {
			struct concrete_pred_expr concrete = (*concrete_exprs)[i];
			bool equality = false;
			switch (concrete.token) {
			case EQUALITY:
				equality = (value == concrete.value);
				break;
			case NULLITY:
				equality = ((void*)value == NULL);
				break;
			default:
				model_print("unknown predicate token");
				break;
			}

			if (equality != concrete.equality) {
				satisfy_predicate = false;
				break;
			}
		}

		if (satisfy_predicate) {
			/* Wake up threads */
			thread_id_t tid = concrete_pred->get_tid();
			Thread * thread = model->get_thread(tid);

			//model_print("** thread %d is woken up\n", thread->get_id());
			((NewFuzzer *)model->get_execution()->getFuzzer())->notify_paused_thread(thread);
		}

		index++;
	}
}

WaitObj * ModelHistory::getWaitObj(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_wait_obj->size();
	if (old_size <= thread_id) {
		thrd_wait_obj->resize(thread_id + 1);
		for (int i = old_size;i < thread_id + 1;i++) {
			(*thrd_wait_obj)[i] = new WaitObj( int_to_id(i) );
		}
	}

	return (*thrd_wait_obj)[thread_id];
}

void ModelHistory::add_waiting_thread(thread_id_t self_id,
																			thread_id_t waiting_for_id, FuncNode * target_node, int dist)
{
	WaitObj * self_wait_obj = getWaitObj(self_id);
	self_wait_obj->add_waiting_for(waiting_for_id, target_node, dist);

	/* Update waited-by relation */
	WaitObj * other_wait_obj = getWaitObj(waiting_for_id);
	other_wait_obj->add_waited_by(self_id);
}

/* Thread tid is woken up (or notified), so it is not waiting for others anymore */
void ModelHistory::remove_waiting_thread(thread_id_t tid)
{
	WaitObj * self_wait_obj = getWaitObj(tid);
	thrd_id_set_t * waiting_for = self_wait_obj->getWaitingFor();

	/* Remove tid from waited_by's */
	thrd_id_set_iter * iter = waiting_for->iterator();
	while (iter->hasNext()) {
		thread_id_t other_id = iter->next();
		WaitObj * other_wait_obj = getWaitObj(other_id);
		other_wait_obj->remove_waited_by(tid);
	}

	self_wait_obj->clear_waiting_for();
	delete iter;
}

void ModelHistory::stop_waiting_for_node(thread_id_t self_id,
																				 thread_id_t waiting_for_id, FuncNode * target_node)
{
	WaitObj * self_wait_obj = getWaitObj(self_id);
	bool thread_removed = self_wait_obj->remove_waiting_for_node(waiting_for_id, target_node);

	// model_print("\t%d gives up %d on node %d\n", self_id, waiting_for_id, target_node->get_func_id());

	/* If thread self_id is not waiting for waiting_for_id anymore */
	if (thread_removed) {
		WaitObj * other_wait_obj = getWaitObj(waiting_for_id);
		other_wait_obj->remove_waited_by(self_id);

		thrd_id_set_t * self_waiting_for = self_wait_obj->getWaitingFor();
		if ( self_waiting_for->isEmpty() ) {
			// model_print("\tthread %d waits for nobody, wake up\n", self_id);
			ModelExecution * execution = model->get_execution();
			Thread * thread = execution->get_thread(self_id);
			((NewFuzzer *)execution->getFuzzer())->notify_paused_thread(thread);
		}
	}
}

bool ModelHistory::skip_action(ModelAction * act)
{
	bool second_part_of_rmw = act->is_rmwc() || act->is_rmw();
	modelclock_t curr_seq_number = act->get_seq_number();

	/* Skip actions that are second part of a read modify write */
	if (second_part_of_rmw)
		return true;

	/* Skip actions with the same sequence number */
	if (last_seq_number != INIT_SEQ_NUMBER && last_seq_number == curr_seq_number)
		return true;

	/* Skip actions that are paused by fuzzer (sequence number is 0) */
	if (curr_seq_number == 0)
		return true;

	return false;
}

/* Monitor thread tid and decide whether other threads (that are waiting for tid)
 * should keep waiting for this thread or not. Shall only be called when a thread
 * enters a function.
 *
 * Heuristics: If the distance from the current FuncNode to some target node
 * ever increases, stop waiting for this thread on this target node.
 */
void ModelHistory::monitor_waiting_thread(uint32_t func_id, thread_id_t tid)
{
	WaitObj * wait_obj = getWaitObj(tid);
	thrd_id_set_t * waited_by = wait_obj->getWaitedBy();
	FuncNode * curr_node = func_nodes[func_id];

	/* For each thread waiting for tid */
	thrd_id_set_iter * tid_iter = waited_by->iterator();
	while (tid_iter->hasNext()) {
		thread_id_t waited_by_id = tid_iter->next();
		WaitObj * other_wait_obj = getWaitObj(waited_by_id);

		node_set_t * target_nodes = other_wait_obj->getTargetNodes(tid);
		node_set_iter * node_iter = target_nodes->iterator();
		while (node_iter->hasNext()) {
			FuncNode * target = node_iter->next();
			int old_dist = other_wait_obj->lookup_dist(tid, target);
			int new_dist = curr_node->compute_distance(target, old_dist);

			if (new_dist == -1) {
				stop_waiting_for_node(waited_by_id, tid, target);
			}
		}

		delete node_iter;
	}

	delete tid_iter;
}

void ModelHistory::monitor_waiting_thread_counter(thread_id_t tid)
{
	WaitObj * wait_obj = getWaitObj(tid);
	thrd_id_set_t * waited_by = wait_obj->getWaitedBy();

	// Thread tid has taken an action, update the counter for threads waiting for tid
	thrd_id_set_iter * tid_iter = waited_by->iterator();
	while (tid_iter->hasNext()) {
		thread_id_t waited_by_id = tid_iter->next();
		WaitObj * other_wait_obj = getWaitObj(waited_by_id);

		bool expire = other_wait_obj->incr_counter(tid);
		if (expire) {
//			model_print("thread %d stops waiting for thread %d\n", waited_by_id, tid);
			wait_obj->remove_waited_by(waited_by_id);
			other_wait_obj->remove_waiting_for(tid);

			thrd_id_set_t * other_waiting_for = other_wait_obj->getWaitingFor();
			if ( other_waiting_for->isEmpty() ) {
				// model_print("\tthread %d waits for nobody, wake up\n", self_id);
				ModelExecution * execution = model->get_execution();
				Thread * thread = execution->get_thread(waited_by_id);
				((NewFuzzer *)execution->getFuzzer())->notify_paused_thread(thread);
			}
		}
	}

	delete tid_iter;
}

/* Reallocate some snapshotted memories when new executions start */
void ModelHistory::set_new_exec_flag()
{
	for (uint i = 1;i < func_nodes.size();i++) {
		FuncNode * func_node = func_nodes[i];
		func_node->set_new_exec_flag();
	}
}

void ModelHistory::dump_func_node_graph()
{
	model_print("digraph func_node_graph {\n");
	for (uint i = 1;i < func_nodes.size();i++) {
		FuncNode * node = func_nodes[i];
		ModelList<FuncNode *> * out_edges = node->get_out_edges();

		model_print("\"%p\" [label=\"%s\"]\n", node, node->get_func_name());
		mllnode<FuncNode *> * it;
		for (it = out_edges->begin();it != NULL;it = it->getNext()) {
			FuncNode * other = it->getVal();
			model_print("\"%p\" -> \"%p\"\n", node, other);
		}
	}
	model_print("}\n");
}

void ModelHistory::print_func_node()
{
	/* function id starts with 1 */
	for (uint32_t i = 1;i < func_nodes.size();i++) {
		FuncNode * func_node = func_nodes[i];
		func_node->print_predicate_tree();

/*
                func_inst_list_mt * entry_insts = func_node->get_entry_insts();
                model_print("function %s has entry actions\n", func_node->get_func_name());

                mllnode<FuncInst*>* it;
                for (it = entry_insts->begin();it != NULL;it=it->getNext()) {
                        FuncInst *inst = it->getVal();
                        model_print("type: %d, at: %s\n", inst->get_type(), inst->get_position());
                }
 */
	}
}

void ModelHistory::print_waiting_threads()
{
	ModelExecution * execution = model->get_execution();
	for (unsigned int i = 0;i < execution->get_num_threads();i++) {
		thread_id_t tid = int_to_id(i);
		WaitObj * wait_obj = getWaitObj(tid);
		wait_obj->print_waiting_for();
	}

	for (unsigned int i = 0;i < execution->get_num_threads();i++) {
		thread_id_t tid = int_to_id(i);
		WaitObj * wait_obj = getWaitObj(tid);
		wait_obj->print_waited_by();
	}
}
