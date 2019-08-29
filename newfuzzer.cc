#include "newfuzzer.h"
#include "threads-model.h"
#include "model.h"
#include "action.h"
#include "execution.h"
#include "funcnode.h"

NewFuzzer::NewFuzzer() :
	thrd_last_read_act(),
	thrd_curr_pred(),
	thrd_selected_child_branch(),
	thrd_pruned_writes()
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
		inst_act_map_t * inst_act_map = func_node->get_inst_act_map(tid);
		Predicate * curr_pred = func_node->get_predicate_tree_position(tid);
		FuncInst * read_inst = func_node->get_inst(read);

		Predicate * selected_branch = selectBranch(tid, curr_pred, read_inst);
		prune_writes(tid, selected_branch, rf_set, inst_act_map);
	}

	// TODO: make this thread sleep if no write satisfies the chosen predicate
	// if no read satisfies the selected predicate
	if ( rf_set->size() == 0 ) {
		SnapVector<ModelAction *> * pruned_writes = thrd_pruned_writes[thread_id];
		for (uint i = 0; i < pruned_writes->size(); i++)
			rf_set->push_back( (*pruned_writes)[i] );
	}

	ASSERT(rf_set->size() != 0);
	int random_index = random() % rf_set->size();

	return random_index;
}

/* Select a random branch from the children of curr_pred 
 * @return The selected branch
 */
Predicate * NewFuzzer::selectBranch(thread_id_t tid, Predicate * curr_pred, FuncInst * read_inst)
{
	int thread_id = id_to_int(tid);
	if ( thrd_selected_child_branch.size() <= (uint) thread_id)
		thrd_selected_child_branch.resize(thread_id + 1);

	if (curr_pred == NULL || read_inst == NULL) {
		thrd_selected_child_branch[thread_id] = NULL;
		return NULL;
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
		return NULL;
	}

	// randomly select a branch
	int random_index = random() % branches.size();
	Predicate * random_branch = branches[ random_index ];
	thrd_selected_child_branch[thread_id] = random_branch;

	return random_branch;
}

Predicate * NewFuzzer::get_selected_child_branch(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	if (thrd_selected_child_branch.size() <= (uint) thread_id)
		return NULL;

	return thrd_selected_child_branch[thread_id];
}

/* Remove writes from the rf_set that do not satisfie the selected predicate, 
 * and store them in thrd_pruned_writes
 *
 * @return true if rf_set is pruned
 */
bool NewFuzzer::prune_writes(thread_id_t tid, Predicate * pred,
	SnapVector<ModelAction *> * rf_set, inst_act_map_t * inst_act_map)
{
	if (pred == NULL)
		return false;

	PredExprSet * pred_expressions = pred->get_pred_expressions();
	if (pred_expressions->getSize() == 0)	// unset predicates
		return false;

	int thread_id = id_to_int(tid);
	bool pruned = false;

	uint old_size = thrd_pruned_writes.size();
	if (thrd_pruned_writes.size() <= (uint) thread_id) {
		uint new_size = thread_id + 1;
		thrd_pruned_writes.resize(new_size);
		for (uint i = old_size; i < new_size; i++)
			thrd_pruned_writes[i] = new SnapVector<ModelAction *>();
	}
	SnapVector<ModelAction *> * pruned_writes = thrd_pruned_writes[thread_id];
	pruned_writes->clear();	// clear the old pruned_writes set

	uint index = 0;
	while ( index < rf_set->size() ) {
		ModelAction * write_act = (*rf_set)[index];
		bool satisfy_predicate = true;

		PredExprSetIter * pred_expr_it = pred_expressions->iterator();
		while (pred_expr_it->hasNext()) {
			struct pred_expr * expression = pred_expr_it->next();
			uint64_t write_val = write_act->get_write_value();
			bool equality;

			// No predicate, return everything in the rf_set
			if (expression->token == NOPREDICATE)
				return pruned;

			switch(expression->token) {
				case EQUALITY:
					FuncInst * to_be_compared;
					ModelAction * last_act;
					uint64_t last_read;

					to_be_compared = expression->func_inst;
					last_act = inst_act_map->get(to_be_compared);
					last_read = last_act->get_reads_from_value();

					equality = (write_val == last_read);
					if (equality != expression->value)
						satisfy_predicate = false;
					break;
				case NULLITY:
					equality = ((void*)write_val == NULL);
					if (equality != expression->value)
						satisfy_predicate = false;
					break;
				default:
					model_print("unknown predicate token\n");
					break;
			}

			if (!satisfy_predicate)
				break;
		}

		if (!satisfy_predicate) {
			ASSERT(rf_set != NULL);
			(*rf_set)[index] = rf_set->back();
			rf_set->pop_back();
			pruned_writes->push_back(write_act);
			pruned = true;
		} else
			index++;
	}

	return pruned;
}
