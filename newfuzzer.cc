#include "newfuzzer.h"
#include "threads-model.h"
#include "model.h"
#include "action.h"
#include "execution.h"
#include "funcnode.h"

/**
 * @brief Register the ModelHistory and ModelExecution engine
 */
void NewFuzzer::register_engine(ModelHistory * history, ModelExecution *execution)
{
	this->history = history;
	this->execution = execution;
}


int NewFuzzer::selectWrite(ModelAction *read, SnapVector<ModelAction *> * rf_set)
{
	thread_id_t tid = read->get_tid();
	int thread_id = id_to_int(tid);

	SnapVector<func_id_list_t> * thrd_func_list = execution->get_thrd_func_list();
	uint32_t func_id = (*thrd_func_list)[thread_id].back();

	FuncNode * func_node = history->get_func_node(func_id);
	FuncInst * read_inst = func_node->get_inst(read);
	Predicate * curr_pred = func_node->get_predicate_tree_position(tid);

	ModelVector<Predicate *> * children = curr_pred->get_children();
	if (children->size() == 0)
		return random() % rf_set->size();

	int random_index = random() % rf_set->size();
	return random_index;
}

