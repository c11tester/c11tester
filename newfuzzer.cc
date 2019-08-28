#include "newfuzzer.h"
#include "threads-model.h"
#include "model.h"
#include "action.h"
#include "execution.h"
#include "funcnode.h"

NewFuzzer::NewFuzzer() :
	thrd_last_read_act(),
	thrd_curr_pred(),
	thrd_selected_child_branch()
{}

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
	int random_index = random() % rf_set->size();

	thread_id_t tid = read->get_tid();
	int thread_id = id_to_int(tid);

	if (thrd_last_read_act.size() <= (uint) thread_id)
		thrd_last_read_act.resize(thread_id + 1);

	// A new read action is encountered, select a random child branch of current predicate
	if (read != thrd_last_read_act[thread_id]) {
		thrd_last_read_act[thread_id] = read;

		SnapVector<func_id_list_t> * thrd_func_list = execution->get_thrd_func_list();
		uint32_t func_id = (*thrd_func_list)[thread_id].back();

		FuncNode * func_node = history->get_func_node(func_id);
		FuncInst * read_inst = func_node->get_inst(read);
		Predicate * curr_pred = func_node->get_predicate_tree_position(tid);
		selectBranch(thread_id, curr_pred, read_inst);
	}

	Predicate * selected_branch = thrd_selected_child_branch[thread_id];
	if (selected_branch == NULL)
		return random_index;

	FuncInst * read_inst = selected_branch->get_func_inst();
	PredExprSet * pred_expressions = selected_branch->get_pred_expressions();

	model_print("thread %d ", tid);
	read_inst->print();

	// unset predicates
	if (pred_expressions->getSize() == 0)
		return random_index;
/*
	PredExprSetIter * pred_expr_it = pred_expressions->iterator();
	while (pred_expr_it->hasNext()) {
		struct pred_expr * expression = pred_expr_it->next();

		switch(expression->token) {
			case NOPREDICATE:
				read_inst->print();
				read->print();
				model_print("no predicate\n");
				return random_index;
			case EQUALITY:
				model_print("equality predicate, under construction\n");
				break;
			case NULLITY:
				model_print("nullity predicate, under construction\n");
				break;
			default:
				model_print("unknown predicate token\n");
				break;
		}
	}
*/
	return random_index;
}

void NewFuzzer::selectBranch(int thread_id, Predicate * curr_pred, FuncInst * read_inst)
{
	if ( thrd_selected_child_branch.size() <= (uint) thread_id)
		thrd_selected_child_branch.resize(thread_id + 1);

	if (read_inst == NULL) {
		thrd_selected_child_branch[thread_id] = NULL;
		return;
	}

	ModelVector<Predicate *> * children = curr_pred->get_children();
	SnapVector<Predicate *> branches;

	for (uint i = 0; i < children->size(); i++) {
		Predicate * child = (*children)[i];
		if (child->get_func_inst() == read_inst)
			branches.push_back(child);
	}

	// predicate children have not been generated
	if (branches.size() == 0) {
		thrd_selected_child_branch[thread_id] = NULL;
		return;
	}

	// randomly select a branch
	int random_index = random() % branches.size();
	Predicate * random_branch = branches[ random_index ];
	thrd_selected_child_branch[thread_id] = random_branch;
}
