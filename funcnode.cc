#include "funcnode.h"

FuncNode::FuncNode() :
	predicate_tree_initialized(false),
	predicate_tree_entry(new Predicate(NULL, true)),
	func_inst_map(),
	inst_list(),
	entry_insts(),
	thrd_read_map()
{
	predicate_tree_entry->add_predicate_expr(NOPREDICATE, NULL, true);
}

/* Check whether FuncInst with the same type, position, and location
 * as act has been added to func_inst_map or not. If not, add it.
 *
 * Note: currently, actions with the same position are filtered out by process_action,
 * so the collision list of FuncInst is not used. May remove it later. 
 */
void FuncNode::add_inst(ModelAction *act)
{
	ASSERT(act);
	const char * position = act->get_position();

	/* THREAD* actions, ATOMIC_LOCK, ATOMIC_TRYLOCK, and ATOMIC_UNLOCK
	 * actions are not tagged with their source line numbers
	 */
	if (position == NULL)
		return;

	if ( func_inst_map.contains(position) ) {
		FuncInst * inst = func_inst_map.get(position);

		ASSERT(inst->get_type() == act->get_type());
		if (inst->get_location() != act->get_location())
			inst->not_single_location();

		return;
	}

	FuncInst * func_inst = new FuncInst(act, this);

	func_inst_map.put(position, func_inst);
	inst_list.push_back(func_inst);
}

/* Get the FuncInst with the same type, position, and location
 * as act
 *
 * @return FuncInst with the same type, position, and location as act */
FuncInst * FuncNode::get_inst(ModelAction *act)
{
	ASSERT(act);
	const char * position = act->get_position();

	/* THREAD* actions, ATOMIC_LOCK, ATOMIC_TRYLOCK, and ATOMIC_UNLOCK
	 * actions are not tagged with their source line numbers
	 */
	if (position == NULL)
		return NULL;

	FuncInst * inst = func_inst_map.get(position);
	if (inst == NULL)
		return NULL;

	action_type inst_type = inst->get_type();
	action_type act_type = act->get_type();

	// else if branch: an RMWRCAS action is converted to a RMW or READ action
	if (inst_type == act_type)
		return inst;
	else if (inst_type == ATOMIC_RMWRCAS &&
			(act_type == ATOMIC_RMW || act_type == ATOMIC_READ))
		return inst;

	return NULL;
}


void FuncNode::add_entry_inst(FuncInst * inst)
{
	if (inst == NULL)
		return;

	mllnode<FuncInst *> * it;
	for (it = entry_insts.begin(); it != NULL; it = it->getNext()) {
		if (inst == it->getVal())
			return;
	}

	entry_insts.push_back(inst);
}

/**
 * @brief Convert ModelAdtion list to FuncInst list 
 * @param act_list A list of ModelActions
 */
void FuncNode::update_tree(action_list_t * act_list)
{
	if (act_list == NULL)
		return;
	else if (act_list->size() == 0)
		return;

	/* build inst_list from act_list for later processing */
	func_inst_list_t inst_list;
	action_list_t read_act_list;

	for (sllnode<ModelAction *> * it = act_list->begin(); it != NULL; it = it->getNext()) {
		ModelAction * act = it->getVal();
		FuncInst * func_inst = get_inst(act);

		if (func_inst == NULL)
			continue;

		inst_list.push_back(func_inst);

		if (func_inst->is_read())
			read_act_list.push_back(act);
	}

	model_print("function %s\n", func_name);
	update_inst_tree(&inst_list);
	update_predicate_tree(&read_act_list);
//	deep_update(predicate_tree_entry);

	print_predicate_tree();
}

/** 
 * @brief Link FuncInsts in inst_list  - add one FuncInst to another's predecessors and successors
 * @param inst_list A list of FuncInsts
 */
void FuncNode::update_inst_tree(func_inst_list_t * inst_list)
{
	if (inst_list == NULL)
		return;
	else if (inst_list->size() == 0)
		return;

	/* start linking */
	sllnode<FuncInst *>* it = inst_list->begin();
	sllnode<FuncInst *>* prev;

	/* add the first instruction to the list of entry insts */
	FuncInst * entry_inst = it->getVal();
	add_entry_inst(entry_inst);

	it = it->getNext();
	while (it != NULL) {
		prev = it->getPrev();

		FuncInst * prev_inst = prev->getVal();
		FuncInst * curr_inst = it->getVal();

		prev_inst->add_succ(curr_inst);
		curr_inst->add_pred(prev_inst);

		it = it->getNext();
	}
}

/* @param tid thread id
 * Store the values read by atomic read actions into thrd_read_map */
void FuncNode::store_read(ModelAction * act, uint32_t tid)
{
	ASSERT(act);

	void * location = act->get_location();
	uint64_t read_from_val = act->get_reads_from_value();

	/* resize and initialize */
	uint32_t old_size = thrd_read_map.size();
	if (old_size <= tid) {
		thrd_read_map.resize(tid + 1);
		for (uint32_t i = old_size; i < tid + 1;i++)
			thrd_read_map[i] = new read_map_t();
	}

	read_map_t * read_map = thrd_read_map[tid];
	read_map->put(location, read_from_val);

	/* Store the memory locations where atomic reads happen */
	// read_locations.add(location);
}

uint64_t FuncNode::query_last_read(void * location, uint32_t tid)
{
	if (thrd_read_map.size() <= tid)
		return VALUE_NONE;

	read_map_t * read_map = thrd_read_map[tid];

	/* last read value not found */
	if ( !read_map->contains(location) )
		return VALUE_NONE;

	uint64_t read_val = read_map->get(location);
	return read_val;
}

/* @param tid thread id
 * Reset read map for a thread. This function shall only be called
 * when a thread exits a function
 */
void FuncNode::clear_read_map(uint32_t tid)
{
	if (thrd_read_map.size() <= tid)
		return;

	thrd_read_map[tid]->reset();
}

void FuncNode::update_predicate_tree(action_list_t * act_list)
{
	if (act_list == NULL || act_list->size() == 0)
		return;
/*
	if (predicate_tree_initialized) {
		return;
	}
	predicate_tree_initialized = true;
*/
	/* map a FuncInst to the its predicate */
	HashTable<FuncInst *, Predicate *, uintptr_t, 0> inst_pred_map(128);
	HashTable<void *, ModelAction *, uintptr_t, 0> loc_act_map(128);
	HashTable<FuncInst *, ModelAction *, uintptr_t, 0> inst_act_map(128);

	sllnode<ModelAction *> *it = act_list->begin();
	Predicate * curr_pred = predicate_tree_entry;
	while (it != NULL) {
		ModelAction * next_act = it->getVal();
		FuncInst * next_inst = get_inst(next_act);
		SnapVector<Predicate *> * unset_predicates = new SnapVector<Predicate *>();

		bool branch_found = follow_branch(&curr_pred, next_inst, next_act, &inst_act_map, unset_predicates);

		/* no predicate, follow the only branch */
		if (!branch_found && unset_predicates->size() != 0) {
			ASSERT(unset_predicates->size() == 1);
			Predicate * one_branch = (*unset_predicates)[0];

			if (!next_inst->is_single_location()) {
				Predicate * another_branch = new Predicate(next_inst);
				// another_branch->copy_predicate_expr(one_branch);

				uint64_t next_read = next_act->get_reads_from_value();
				bool isnull = ((void*)next_read == NULL);
				if (isnull) {
					one_branch->add_predicate_expr(NULLITY, NULL, 1);
					another_branch->add_predicate_expr(NULLITY, NULL, 0);
				} else {
					another_branch->add_predicate_expr(NULLITY, NULL, 1);
					one_branch->add_predicate_expr(NULLITY, NULL, 0);
				}

				curr_pred->add_child(another_branch);
				another_branch->set_parent(curr_pred);
			}

			curr_pred = one_branch;
			branch_found = true;
		}

		delete unset_predicates;

		// check back edges
		if (!branch_found) {
			bool backedge_found = false;
			Predicate * back_pred = curr_pred->get_backedge();
			if (back_pred != NULL) {
				curr_pred = back_pred;
				backedge_found = true;
			} else if (inst_pred_map.contains(next_inst)) {
				inst_pred_map.remove(curr_pred->get_func_inst());
				Predicate * old_pred = inst_pred_map.get(next_inst);
				back_pred = old_pred->get_parent();

				curr_pred->set_backedge(back_pred);
				curr_pred = back_pred;
				backedge_found = true;
			}

			if (backedge_found)
				continue;
		}

		if (!branch_found) {
			if ( loc_act_map.contains(next_act->get_location()) ) {
				ModelAction * last_act = loc_act_map.get(next_act->get_location());
				FuncInst * last_inst = get_inst(last_act);

				Predicate * new_pred1 = new Predicate(next_inst);
				new_pred1->add_predicate_expr(EQUALITY, last_inst, true);

				Predicate * new_pred2 = new Predicate(next_inst);
				new_pred2->add_predicate_expr(EQUALITY, last_inst, false);

				curr_pred->add_child(new_pred1);
				curr_pred->add_child(new_pred2);
				new_pred1->set_parent(curr_pred);
				new_pred2->set_parent(curr_pred);

				uint64_t last_read = last_act->get_reads_from_value();
				uint64_t next_read = next_act->get_reads_from_value();

				if ( last_read == next_read )
					curr_pred = new_pred1;
				else
					curr_pred = new_pred2;
			} else {
				Predicate * new_pred = new Predicate(next_inst);
				curr_pred->add_child(new_pred);
				new_pred->set_parent(curr_pred);

				if (curr_pred->is_entry_predicate())
					new_pred->add_predicate_expr(NOPREDICATE, NULL, true);

				curr_pred = new_pred;
			}
		}

		if (!inst_pred_map.contains(next_inst))
			inst_pred_map.put(next_inst, curr_pred);

		loc_act_map.put(next_act->get_location(), next_act);
		inst_act_map.put(next_inst, next_act);
		it = it->getNext();
	}
}

void FuncNode::deep_update(Predicate * curr_pred)
{
	FuncInst * func_inst = curr_pred->get_func_inst();
	if (func_inst != NULL && !func_inst->is_single_location()) {
		bool has_null_pred = false;
		PredExprSet * pred_expressions = curr_pred->get_pred_expressions();
		PredExprSetIter * pred_expr_it = pred_expressions->iterator();
		while (pred_expr_it->hasNext()) {
			pred_expr * pred_expression = pred_expr_it->next();
			if (pred_expression->token == NULLITY) {
				has_null_pred = true;
				break;
			}
		}

		if (!has_null_pred) {
//			func_inst->print();
			Predicate * another_branch = new Predicate(func_inst);
			another_branch->copy_predicate_expr(curr_pred);
			another_branch->add_predicate_expr(NULLITY, NULL, 1);
			curr_pred->add_predicate_expr(NULLITY, NULL, 0);

			Predicate * parent = curr_pred->get_parent();
			parent->add_child(another_branch);
//			another_branch.add_children(i);
		}
	}

	ModelVector<Predicate *> * branches = curr_pred->get_children();
	for (uint i = 0; i < branches->size(); i++) {
		Predicate * branch = (*branches)[i];
		deep_update(branch);
	}
}

/* Given curr_pred and next_inst, find the branch following curr_pred that
 * contains next_inst and the correct predicate. 
 * @return true if branch found, false otherwise.
 */
bool FuncNode::follow_branch(Predicate ** curr_pred, FuncInst * next_inst, ModelAction * next_act,
	HashTable<FuncInst *, ModelAction *, uintptr_t, 0> * inst_act_map,
	SnapVector<Predicate *> * unset_predicates)
{
	/* check if a branch with func_inst and corresponding predicate exists */
	bool branch_found = false;
	ModelVector<Predicate *> * branches = (*curr_pred)->get_children();
	for (uint i = 0; i < branches->size(); i++) {
		Predicate * branch = (*branches)[i];
		if (branch->get_func_inst() != next_inst)
			continue;

		/* check against predicate expressions */
		bool predicate_correct = true;
		PredExprSet * pred_expressions = branch->get_pred_expressions();
		PredExprSetIter * pred_expr_it = pred_expressions->iterator();

		if (pred_expressions->getSize() == 0) {
			predicate_correct = false;
			unset_predicates->push_back(branch);
		}

		while (pred_expr_it->hasNext()) {
			pred_expr * pred_expression = pred_expr_it->next();
			uint64_t last_read, next_read;
			bool equality;

			switch(pred_expression->token) {
				case NOPREDICATE:
					predicate_correct = true;
					break;
				case EQUALITY:
					FuncInst * to_be_compared;
					ModelAction * last_act;

					to_be_compared = pred_expression->func_inst;
					last_act = inst_act_map->get(to_be_compared);

					last_read = last_act->get_reads_from_value();
					next_read = next_act->get_reads_from_value();
					equality = (last_read == next_read);
					if (equality != pred_expression->value)
						predicate_correct = false;

					break;
				case NULLITY:
					next_read = next_act->get_reads_from_value();
					equality = ((void*)next_read == NULL);
					if (equality != pred_expression->value)
						predicate_correct = false;
					break;
				default:
					predicate_correct = false;
					model_print("unkown predicate token\n");
					break;
			}
		}

		if (predicate_correct) {
			*curr_pred = branch;
			branch_found = true;
			break;
		}
	}

	return branch_found;
}

void FuncNode::print_predicate_tree()
{
	model_print("digraph function_%s {\n", func_name);
	predicate_tree_entry->print_pred_subtree();
	model_print("}\n");	// end of graph
}

/* @param tid thread id
 * Print the values read by the last read actions for each memory location
 */
/*
void FuncNode::print_last_read(uint32_t tid)
{
	ASSERT(thrd_read_map.size() > tid);
	read_map_t * read_map = thrd_read_map[tid];

	mllnode<void *> * it;
	for (it = read_locations.begin();it != NULL;it=it->getNext()) {
		if ( !read_map->contains(it->getVal()) )
			break;

		uint64_t read_val = read_map->get(it->getVal());
		model_print("last read of thread %d at %p: 0x%x\n", tid, it->getVal(), read_val);
	}
}
*/
