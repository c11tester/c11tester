#include "newfuzzer.h"
#include "threads-model.h"
#include "model.h"
#include "action.h"
#include "execution.h"
#include "funcnode.h"

typedef HashTable<FuncInst *, ModelAction *, uintptr_t, 0> inst_act_map_t;

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

	SnapVector<func_id_list_t> * thrd_func_list = execution->get_thrd_func_list();
	uint32_t func_id = (*thrd_func_list)[thread_id].back();
	FuncNode * func_node = history->get_func_node(func_id);
	inst_act_map_t * inst_act_map = func_node->get_inst_act_map(tid);

	// A new read action is encountered, select a random child branch of current predicate
	if (read != thrd_last_read_act[thread_id]) {
		thrd_last_read_act[thread_id] = read;

		FuncInst * read_inst = func_node->get_inst(read);
		Predicate * curr_pred = func_node->get_predicate_tree_position(tid);
		selectBranch(thread_id, curr_pred, read_inst);
	}

	Predicate * selected_branch = thrd_selected_child_branch[thread_id];
	if (selected_branch == NULL)
		return random_index;

	PredExprSet * pred_expressions = selected_branch->get_pred_expressions();

//	FuncInst * read_inst = selected_branch->get_func_inst();
//	model_print("thread %d ", tid);
//	read_inst->print();

	// unset predicates
	if (pred_expressions->getSize() == 0)
		return random_index;

	for (uint index = 0; index < rf_set->size(); index++) {
		ModelAction * write_act = (*rf_set)[index];
		bool satisfy_predicate = true;

		PredExprSetIter * pred_expr_it = pred_expressions->iterator();
		while (pred_expr_it->hasNext()) {
			struct pred_expr * expression = pred_expr_it->next();
			uint64_t last_read, write_val;
			bool equality;

			if (expression->token == NOPREDICATE)
				return random_index;

			switch(expression->token) {
				case EQUALITY:
					FuncInst * to_be_compared;
					ModelAction * last_act;

					to_be_compared = expression->func_inst;
					last_act = inst_act_map->get(to_be_compared);

					last_read = last_act->get_reads_from_value();
					write_val = write_act->get_write_value();

					equality = (last_read == write_val);
					if (equality != expression->value)
						satisfy_predicate = false;

					model_print("equality predicate\n");
					break;
				case NULLITY:
					model_print("nullity predicate, under construction\n");
					break;
				default:
					model_print("unknown predicate token\n");
					break;
			}

			if (!satisfy_predicate)
				break;
		}

		/* TODO: collect all writes that satisfy predicate; if some of them violate
		 * modification graph, others can be chosen */
		if (satisfy_predicate) {
			model_print("^_^ satisfy predicate\n");
			return index;
		}
	}

	// TODO: make this thread sleep if no write satisfies the chosen predicate
	return random_index;
}

void NewFuzzer::selectBranch(int thread_id, Predicate * curr_pred, FuncInst * read_inst)
{
	if ( thrd_selected_child_branch.size() <= (uint) thread_id)
		thrd_selected_child_branch.resize(thread_id + 1);

	if (curr_pred == NULL || read_inst == NULL) {
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

Predicate * NewFuzzer::get_selected_child_branch(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	if (thrd_selected_child_branch.size() <= (uint) thread_id)
		return NULL;

	return thrd_selected_child_branch[thread_id];
}
