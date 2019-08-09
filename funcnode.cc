#include "funcnode.h"
#include <fcntl.h>

FuncNode::FuncNode() :
	predicate_tree_initialized(false),
	predicate_tree_entry(new Predicate(NULL, true)),
	func_inst_map(),
	inst_list(),
	entry_insts(),
	thrd_read_map()
{}

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

		if (inst->get_type() != act->get_type() ) {
			// model_print("action with a different type occurs at line number %s\n", position);
			FuncInst * func_inst = inst->search_in_collision(act);

			if (func_inst != NULL)
				return;

			func_inst = new FuncInst(act, this);
			inst->get_collisions()->push_back(func_inst);
			inst_list.push_back(func_inst);	// delete?
		}

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
	HashTable<ModelAction *, FuncInst *, uintptr_t, 4> act_inst_map(128);

	for (sllnode<ModelAction *> * it = act_list->begin(); it != NULL; it = it->getNext()) {
		ModelAction * act = it->getVal();
		FuncInst * func_inst = get_inst(act);

		if (func_inst == NULL)
			continue;

		inst_list.push_back(func_inst);

//		model_print("position: %s ", act->get_position());
//		act->print();

		if (func_inst->is_read()) {
			read_act_list.push_back(act);
			act_inst_map.put(act, func_inst);
		}
	}

	update_inst_tree(&inst_list);
	update_predicate_tree(&read_act_list, &act_inst_map);
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
		return 0xdeadbeef;

	read_map_t * read_map = thrd_read_map[tid];

	/* last read value not found */
	if ( !read_map->contains(location) )
		return 0xdeadbeef;

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

void FuncNode::update_predicate_tree(action_list_t * act_list, HashTable<ModelAction *, FuncInst *, uintptr_t, 4> * act_inst_map)
{
	if (act_list == NULL || act_list->size() == 0)
		return;
/*
	if (predicate_tree_initialized) {
		return;
	}
	predicate_tree_initialized = true;
*/
	/* map a FuncInst to the parent of its predicate */
	HashTable<FuncInst *, Predicate *, uintptr_t, 0> inst_pred_map(128);
	HashTable<FuncInst *, uint64_t, uintptr_t, 4> read_val_map(128);
	HashTable<void *, FuncInst *, uintptr_t, 4> loc_inst_map(128);

	sllnode<ModelAction *> *it = act_list->begin();
	Predicate * curr_pred = predicate_tree_entry;

	while (it != NULL) {
		ModelAction * curr_act = it->getVal();
		FuncInst * curr_inst = act_inst_map->get(curr_act);
		Predicate * old_pred = curr_pred;
		read_val_map.put(curr_inst, curr_act->get_reads_from_value());

		bool branch_found = follow_branch(&curr_pred, curr_inst, &read_val_map, &loc_inst_map);

		// check back edges
		if (!branch_found) {
			Predicate * back_pred = curr_pred->get_backedge();
			if (back_pred != NULL) {
				curr_pred = back_pred;
				continue;
			}

			if (inst_pred_map.contains(curr_inst)) {
				back_pred = inst_pred_map.get(curr_inst);
				curr_pred->set_backedge(back_pred);
				curr_pred = back_pred;
				continue;
			}
		}

		if (!inst_pred_map.contains(curr_inst))
			inst_pred_map.put(curr_inst, old_pred);

		if (!branch_found) {
			if ( loc_inst_map.contains(curr_inst->get_location()) ) {
				Predicate * new_pred1 = new Predicate(curr_inst);
				new_pred1->add_predicate(EQUALITY, curr_inst->get_location(), true);

				Predicate * new_pred2 = new Predicate(curr_inst);
				new_pred2->add_predicate(EQUALITY, curr_inst->get_location(), false);

				curr_pred->add_child(new_pred1);
				curr_pred->add_child(new_pred2);
				//new_pred1->add_parent(curr_pred);
				//new_pred2->add_parent(curr_pred);

				FuncInst * last_inst = loc_inst_map.get(curr_inst->get_location());
				uint64_t last_read = read_val_map.get(last_inst);
				if ( last_read == read_val_map.get(curr_inst) )
					curr_pred = new_pred1;
				else
					curr_pred = new_pred2;
			} else {
				Predicate * new_pred = new Predicate(curr_inst);
				curr_pred->add_child(new_pred);
				//new_pred->add_parent(curr_pred);

				curr_pred = new_pred;
			}
		}

		loc_inst_map.put(curr_inst->get_location(), curr_inst);
		it = it->getNext();
	}

//	model_print("function %s\n", func_name);
//	print_predicate_tree();
}

/* Given curr_pred and next_inst, find the branch following curr_pred that contains next_inst and the correct predicate
 * @return true if branch found, false otherwise.
 */
bool FuncNode::follow_branch(Predicate ** curr_pred, FuncInst * next_inst,
	HashTable<FuncInst *, uint64_t, uintptr_t, 4> * read_val_map, HashTable<void *, FuncInst *, uintptr_t, 4> * loc_inst_map)
{
	/* check if a branch with func_inst and corresponding predicate exists */
	bool branch_found = false;
	ModelVector<Predicate *> * branches = (*curr_pred)->get_children();
	for (uint i = 0; i < branches->size(); i++) {
		Predicate * branch = (*branches)[i];
		if (branch->get_func_inst() != next_inst)
			continue;

		PredExprSet * pred_expressions = branch->get_pred_expressions();

		/* no predicate, follow the only branch */
		if (pred_expressions->getSize() == 0) {
//			model_print("no predicate exists: "); next_inst->print();
			*curr_pred = branch;
			branch_found = true;
			break;
		}

		PredExprSetIter * pred_expr_it = pred_expressions->iterator();
		while (pred_expr_it->hasNext()) {
			pred_expr * pred_expression = pred_expr_it->next();
			uint64_t last_read, next_read;
			FuncInst * last_inst;
			bool equality;

			switch(pred_expression->token) {
				case EQUALITY:
					last_inst = loc_inst_map->get(next_inst->get_location());
					last_read = read_val_map->get(last_inst);
					next_read = read_val_map->get(next_inst);
					equality = (last_read == next_read);

					if (equality == pred_expression->value) {
						*curr_pred = branch;
//						model_print("predicate: token: %d, location: %p, value: %d - ", pred_expression->token, pred_expression->location, pred_expression->value); next_inst->print();
						branch_found = true;
					}
					break;
				case NULLITY:
					break;
				default:
					model_print("unkown predicate token\n");
					break;
			}
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
