#include "action.h"
#include "history.h"
#include "funcnode.h"
#include "funcinst.h"
#include "predicate.h"
#include "concretepredicate.h"

#include "model.h"
#include <cmath>

FuncNode::FuncNode(ModelHistory * history) :
	history(history),
	exit_count(0),
	marker(1),
	func_inst_map(),
	inst_list(),
	entry_insts(),
	inst_pred_map(128),
	inst_id_map(128),
	loc_act_map(128),
	predicate_tree_position(),
	predicate_leaves(),
	leaves_tmp_storage(),
	weight_debug_vec(),
	failed_predicates(),
	edge_table(32),
	out_edges()
{
	predicate_tree_entry = new Predicate(NULL, true);
	predicate_tree_entry->add_predicate_expr(NOPREDICATE, NULL, true);

	predicate_tree_exit = new Predicate(NULL, false, true);
	predicate_tree_exit->set_depth(MAX_DEPTH);

	/* Snapshot data structures below */
	action_list_buffer = new SnapList<action_list_t *>();
	read_locations = new loc_set_t();
	write_locations = new loc_set_t();
	val_loc_map = new HashTable<uint64_t, loc_set_t *, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free, int64_hash>();
	loc_may_equal_map = new HashTable<void *, loc_set_t *, uintptr_t, 0>();

	//values_may_read_from = new value_set_t();
}

/* Reallocate snapshotted memories when new executions start */
void FuncNode::set_new_exec_flag()
{
	action_list_buffer = new SnapList<action_list_t *>();
	read_locations = new loc_set_t();
	write_locations = new loc_set_t();
	val_loc_map = new HashTable<uint64_t, loc_set_t *, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free, int64_hash>();
	loc_may_equal_map = new HashTable<void *, loc_set_t *, uintptr_t, 0>();

	//values_may_read_from = new value_set_t();
}

/* Check whether FuncInst with the same type, position, and location
 * as act has been added to func_inst_map or not. If not, add it.
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

	FuncInst * func_inst = func_inst_map.get(position);

	/* This position has not been inserted into hashtable before */
	if (func_inst == NULL) {
		func_inst = create_new_inst(act);
		func_inst_map.put(position, func_inst);
		return;
	}

	/* Volatile variables that use ++ or -- syntax may result in read and write actions with the same position */
	if (func_inst->get_type() != act->get_type()) {
		FuncInst * collision_inst = func_inst->search_in_collision(act);

		if (collision_inst == NULL) {
			collision_inst = create_new_inst(act);
			func_inst->add_to_collision(collision_inst);
			return;
		} else {
			func_inst = collision_inst;
		}
	}

	ASSERT(func_inst->get_type() == act->get_type());
	int curr_execution_number = model->get_execution_number();

	/* Reset locations when new executions start */
	if (func_inst->get_execution_number() != curr_execution_number) {
		func_inst->set_location(act->get_location());
		func_inst->set_execution_number(curr_execution_number);
	}

	/* Mark the memory location of such inst as not unique */
	if (func_inst->get_location() != act->get_location())
		func_inst->not_single_location();
}

FuncInst * FuncNode::create_new_inst(ModelAction * act)
{
	FuncInst * func_inst = new FuncInst(act, this);
	int exec_num = model->get_execution_number();
	func_inst->set_execution_number(exec_num);

	inst_list.push_back(func_inst);

	return func_inst;
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

	if (inst_type == act_type) {
		return inst;
	}
	/* RMWRCAS actions are converted to RMW or READ actions */
	else if (inst_type == ATOMIC_RMWRCAS &&
					 (act_type == ATOMIC_RMW || act_type == ATOMIC_READ)) {
		return inst;
	}
	/* Return the FuncInst in the collision list */
	else {
		return inst->search_in_collision(act);
	}
}


void FuncNode::add_entry_inst(FuncInst * inst)
{
	if (inst == NULL)
		return;

	mllnode<FuncInst *> * it;
	for (it = entry_insts.begin();it != NULL;it = it->getNext()) {
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
	if (act_list == NULL || act_list->size() == 0)
		return;

	HashTable<void *, value_set_t *, uintptr_t, 0> * write_history = history->getWriteHistory();
	HashSet<ModelAction *, uintptr_t, 2> write_actions;

	/* build inst_list from act_list for later processing */
	func_inst_list_t inst_list;
	action_list_t rw_act_list;

	for (sllnode<ModelAction *> * it = act_list->begin();it != NULL;it = it->getNext()) {
		ModelAction * act = it->getVal();

		// Use the original action type and decrement ref count
		// so that actions may be deleted by Execution::collectActions
		if (act->get_original_type() != ATOMIC_NOP && act->get_swap_flag() == false)
			act->use_original_type();

		act->decr_func_ref_count();

		if (act->is_read()) {
			// For every read or rmw actions in this list, the reads_from was marked, and not deleted.
			// So it is safe to call get_reads_from
			ModelAction * rf = act->get_reads_from();
			if (rf->get_original_type() != ATOMIC_NOP && rf->get_swap_flag() == false)
				rf->use_original_type();

			rf->decr_func_ref_count();
		}

		FuncInst * func_inst = get_inst(act);
		void * loc = act->get_location();

		if (func_inst == NULL)
			continue;

		inst_list.push_back(func_inst);
		bool act_added = false;

		if (act->is_write()) {
			rw_act_list.push_back(act);
			act_added = true;
			if (!write_locations->contains(loc)) {
				write_locations->add(loc);
				history->update_loc_wr_func_nodes_map(loc, this);
			}
		}

		if (act->is_read()) {
			if (!act_added)
				rw_act_list.push_back(act);

			/* If func_inst may only read_from a single location, then:
			 *
			 * The first time an action reads from some location,
			 * import all the values that have been written to this
			 * location from ModelHistory and notify ModelHistory
			 * that this FuncNode may read from this location.
			 */
			if (!read_locations->contains(loc) && func_inst->is_single_location()) {
				read_locations->add(loc);
				value_set_t * write_values = write_history->get(loc);
				add_to_val_loc_map(write_values, loc);
				history->update_loc_rd_func_nodes_map(loc, this);
			}
		}
	}

	update_inst_tree(&inst_list);
	update_predicate_tree(&rw_act_list);

	// Revert back action types and free
	for (sllnode<ModelAction *> * it = act_list->begin(); it != NULL;) {
		ModelAction * act = it->getVal();
		// Do iteration early in case we delete read actions
		it = it->getNext();

		// Collect write actions and reads_froms
		if (act->is_read()) {
			if (act->is_rmw()) {
				write_actions.add(act);
			}

			ModelAction * rf = act->get_reads_from();
			write_actions.add(rf);
		} else if (act->is_write()) {
			write_actions.add(act);
		}

		// Revert back action types
		if (act->is_read()) {
			ModelAction * rf = act->get_reads_from();
			if (rf->get_swap_flag() == true)
				rf->use_original_type();
		}

		if (act->get_swap_flag() == true)
			act->use_original_type();

		//  Free read actions
		if (act->is_read()) {
			if (act->is_rmw()) {
				// Do nothing. Its reads_from can not be READY_FREE
			} else {
				ModelAction *rf = act->get_reads_from();
				if (rf->is_free()) {
					model_print("delete read %d; %p\n", act->get_seq_number(), act);
					delete act;
				}
			}
		}
	}

	// Free write actions if possible
	HSIterator<ModelAction *, uintptr_t, 2> * it = write_actions.iterator();
	while (it->hasNext()) {
		ModelAction * act = it->next();

		if (act->is_free() && act->get_func_ref_count() == 0)
			delete act;
	}
	delete it;

//	print_predicate_tree();
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

void FuncNode::update_predicate_tree(action_list_t * act_list)
{
	if (act_list == NULL || act_list->size() == 0)
		return;

	incr_marker();
	uint32_t inst_counter = 0;

	// Clear hashtables
	loc_act_map.reset();
	inst_pred_map.reset();
	inst_id_map.reset();

	// Clear the set of leaves encountered in this path
	leaves_tmp_storage.clear();

	sllnode<ModelAction *> *it = act_list->begin();
	Predicate * curr_pred = predicate_tree_entry;
	while (it != NULL) {
		ModelAction * next_act = it->getVal();
		FuncInst * next_inst = get_inst(next_act);
		next_inst->set_associated_act(next_act, marker);

		Predicate * unset_predicate = NULL;
		bool branch_found = follow_branch(&curr_pred, next_inst, next_act, &unset_predicate);

		// A branch with unset predicate expression is detected
		if (!branch_found && unset_predicate != NULL) {
			bool amended = amend_predicate_expr(curr_pred, next_inst, next_act);
			if (amended)
				continue;
			else {
				curr_pred = unset_predicate;
				branch_found = true;
			}
		}

		// Detect loops
		if (!branch_found && inst_id_map.contains(next_inst)) {
			FuncInst * curr_inst = curr_pred->get_func_inst();
			uint32_t curr_id = inst_id_map.get(curr_inst);
			uint32_t next_id = inst_id_map.get(next_inst);

			if (curr_id >= next_id) {
				Predicate * old_pred = inst_pred_map.get(next_inst);
				Predicate * back_pred = old_pred->get_parent();

				// For updating weights
				leaves_tmp_storage.push_back(curr_pred);

				// Add to the set of backedges
				curr_pred->add_backedge(back_pred);
				curr_pred = back_pred;
				continue;
			}
		}

		// Generate new branches
		if (!branch_found) {
			SnapVector<struct half_pred_expr *> half_pred_expressions;
			infer_predicates(next_inst, next_act, &half_pred_expressions);
			generate_predicates(curr_pred, next_inst, &half_pred_expressions);
			continue;
		}

		if (next_act->is_write())
			curr_pred->set_write(true);

		if (next_act->is_read()) {
			/* Only need to store the locations of read actions */
			loc_act_map.put(next_act->get_location(), next_act);
		}

		inst_pred_map.put(next_inst, curr_pred);
		if (!inst_id_map.contains(next_inst))
			inst_id_map.put(next_inst, inst_counter++);

		it = it->getNext();
		curr_pred->incr_expl_count();
	}

	if (curr_pred->get_exit() == NULL) {
		// Exit predicate is unset yet
		curr_pred->set_exit(predicate_tree_exit);
	}

	leaves_tmp_storage.push_back(curr_pred);
	update_predicate_tree_weight();
}

/* Given curr_pred and next_inst, find the branch following curr_pred that
 * contains next_inst and the correct predicate.
 * @return true if branch found, false otherwise.
 */
bool FuncNode::follow_branch(Predicate ** curr_pred, FuncInst * next_inst,
														 ModelAction * next_act, Predicate ** unset_predicate)
{
	/* Check if a branch with func_inst and corresponding predicate exists */
	bool branch_found = false;
	ModelVector<Predicate *> * branches = (*curr_pred)->get_children();
	for (uint i = 0;i < branches->size();i++) {
		Predicate * branch = (*branches)[i];
		if (branch->get_func_inst() != next_inst)
			continue;

		/* Check against predicate expressions */
		bool predicate_correct = true;
		PredExprSet * pred_expressions = branch->get_pred_expressions();

		/* Only read and rmw actions my have unset predicate expressions */
		if (pred_expressions->getSize() == 0) {
			predicate_correct = false;
			if (*unset_predicate == NULL)
				*unset_predicate = branch;
			else
				ASSERT(false);

			continue;
		}

		PredExprSetIter * pred_expr_it = pred_expressions->iterator();
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
				last_act = to_be_compared->get_associated_act(marker);

				last_read = last_act->get_reads_from_value();
				next_read = next_act->get_reads_from_value();
				equality = (last_read == next_read);
				if (equality != pred_expression->value)
					predicate_correct = false;

				break;
			case NULLITY:
				next_read = next_act->get_reads_from_value();
				// TODO: implement likely to be null
				equality = ( (void*) (next_read & 0xffffffff) == NULL);
				if (equality != pred_expression->value)
					predicate_correct = false;
				break;
			default:
				predicate_correct = false;
				model_print("unkown predicate token\n");
				break;
			}
		}

		delete pred_expr_it;

		if (predicate_correct) {
			*curr_pred = branch;
			branch_found = true;
			break;
		}
	}

	return branch_found;
}

/* Infer predicate expressions, which are generated in FuncNode::generate_predicates */
void FuncNode::infer_predicates(FuncInst * next_inst, ModelAction * next_act,
																SnapVector<struct half_pred_expr *> * half_pred_expressions)
{
	void * loc = next_act->get_location();

	if (next_inst->is_read()) {
		/* read + rmw */
		if ( loc_act_map.contains(loc) ) {
			ModelAction * last_act = loc_act_map.get(loc);
			FuncInst * last_inst = get_inst(last_act);
			struct half_pred_expr * expression = new half_pred_expr(EQUALITY, last_inst);
			half_pred_expressions->push_back(expression);
		} else if ( next_inst->is_single_location() ) {
			loc_set_t * loc_may_equal = loc_may_equal_map->get(loc);

			if (loc_may_equal != NULL) {
				loc_set_iter * loc_it = loc_may_equal->iterator();
				while (loc_it->hasNext()) {
					void * neighbor = loc_it->next();
					if (loc_act_map.contains(neighbor)) {
						ModelAction * last_act = loc_act_map.get(neighbor);
						FuncInst * last_inst = get_inst(last_act);

						struct half_pred_expr * expression = new half_pred_expr(EQUALITY, last_inst);
						half_pred_expressions->push_back(expression);
					}
				}

				delete loc_it;
			}
		} else {
			// next_inst is not single location
			uint64_t read_val = next_act->get_reads_from_value();

			// only infer NULLITY predicate when it is actually NULL.
			if ( (void*)read_val == NULL) {
				struct half_pred_expr * expression = new half_pred_expr(NULLITY, NULL);
				half_pred_expressions->push_back(expression);
			}
		}
	} else {
		/* Pure writes */
		// TODO: do anything here?
	}
}

/* Able to generate complex predicates when there are multiple predciate expressions */
void FuncNode::generate_predicates(Predicate * curr_pred, FuncInst * next_inst,
																	 SnapVector<struct half_pred_expr *> * half_pred_expressions)
{
	if (half_pred_expressions->size() == 0) {
		Predicate * new_pred = new Predicate(next_inst);
		curr_pred->add_child(new_pred);
		new_pred->set_parent(curr_pred);

		/* Maintain predicate leaves */
		predicate_leaves.add(new_pred);
		predicate_leaves.remove(curr_pred);

		/* entry predicates and predicates containing pure write actions
		 * have no predicate expressions */
		if ( curr_pred->is_entry_predicate() )
			new_pred->add_predicate_expr(NOPREDICATE, NULL, true);
		else if (next_inst->is_write()) {
			/* next_inst->is_write() <==> pure writes */
			new_pred->add_predicate_expr(NOPREDICATE, NULL, true);
		}

		return;
	}

	SnapVector<Predicate *> predicates;

	struct half_pred_expr * half_expr = (*half_pred_expressions)[0];
	predicates.push_back(new Predicate(next_inst));
	predicates.push_back(new Predicate(next_inst));

	predicates[0]->add_predicate_expr(half_expr->token, half_expr->func_inst, true);
	predicates[1]->add_predicate_expr(half_expr->token, half_expr->func_inst, false);

	for (uint i = 1;i < half_pred_expressions->size();i++) {
		half_expr = (*half_pred_expressions)[i];

		uint old_size = predicates.size();
		for (uint j = 0;j < old_size;j++) {
			Predicate * pred = predicates[j];
			Predicate * new_pred = new Predicate(next_inst);
			new_pred->copy_predicate_expr(pred);

			pred->add_predicate_expr(half_expr->token, half_expr->func_inst, true);
			new_pred->add_predicate_expr(half_expr->token, half_expr->func_inst, false);

			predicates.push_back(new_pred);
		}
	}

	for (uint i = 0;i < predicates.size();i++) {
		Predicate * pred= predicates[i];
		curr_pred->add_child(pred);
		pred->set_parent(curr_pred);

		/* Add new predicate leaves */
		predicate_leaves.add(pred);
	}

	/* Remove predicate node that has children */
	predicate_leaves.remove(curr_pred);

	/* Free memories allocated by infer_predicate */
	for (uint i = 0;i < half_pred_expressions->size();i++) {
		struct half_pred_expr * tmp = (*half_pred_expressions)[i];
		snapshot_free(tmp);
	}
}

/* Amend predicates that contain no predicate expressions. Currenlty only amend with NULLITY predicates */
bool FuncNode::amend_predicate_expr(Predicate * curr_pred, FuncInst * next_inst, ModelAction * next_act)
{
	ModelVector<Predicate *> * children = curr_pred->get_children();

	Predicate * unset_pred = NULL;
	for (uint i = 0;i < children->size();i++) {
		Predicate * child = (*children)[i];
		if (child->get_func_inst() == next_inst) {
			unset_pred = child;
			break;
		}
	}

	uint64_t read_val = next_act->get_reads_from_value();

	// only generate NULLITY predicate when it is actually NULL.
	if ( !next_inst->is_single_location() && (void*)read_val == NULL ) {
		Predicate * new_pred = new Predicate(next_inst);

		curr_pred->add_child(new_pred);
		new_pred->set_parent(curr_pred);

		unset_pred->add_predicate_expr(NULLITY, NULL, false);
		new_pred->add_predicate_expr(NULLITY, NULL, true);

		return true;
	}

	return false;
}

void FuncNode::add_to_val_loc_map(uint64_t val, void * loc)
{
	loc_set_t * locations = val_loc_map->get(val);

	if (locations == NULL) {
		locations = new loc_set_t();
		val_loc_map->put(val, locations);
	}

	update_loc_may_equal_map(loc, locations);
	locations->add(loc);
	// values_may_read_from->add(val);
}

void FuncNode::add_to_val_loc_map(value_set_t * values, void * loc)
{
	if (values == NULL)
		return;

	value_set_iter * it = values->iterator();
	while (it->hasNext()) {
		uint64_t val = it->next();
		add_to_val_loc_map(val, loc);
	}

	delete it;
}

void FuncNode::update_loc_may_equal_map(void * new_loc, loc_set_t * old_locations)
{
	if ( old_locations->contains(new_loc) )
		return;

	loc_set_t * neighbors = loc_may_equal_map->get(new_loc);

	if (neighbors == NULL) {
		neighbors = new loc_set_t();
		loc_may_equal_map->put(new_loc, neighbors);
	}

	loc_set_iter * loc_it = old_locations->iterator();
	while (loc_it->hasNext()) {
		// new_loc: { old_locations, ... }
		void * member = loc_it->next();
		neighbors->add(member);

		// for each i in old_locations, i : { new_loc, ... }
		loc_set_t * _neighbors = loc_may_equal_map->get(member);
		if (_neighbors == NULL) {
			_neighbors = new loc_set_t();
			loc_may_equal_map->put(member, _neighbors);
		}
		_neighbors->add(new_loc);
	}

	delete loc_it;
}

/* Every time a thread enters a function, set its position to the predicate tree entry */
void FuncNode::init_predicate_tree_position(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	if (predicate_tree_position.size() <= (uint) thread_id)
		predicate_tree_position.resize(thread_id + 1);

	predicate_tree_position[thread_id] = predicate_tree_entry;
}

void FuncNode::set_predicate_tree_position(thread_id_t tid, Predicate * pred)
{
	int thread_id = id_to_int(tid);
	predicate_tree_position[thread_id] = pred;
}

/* @return The position of a thread in a predicate tree */
Predicate * FuncNode::get_predicate_tree_position(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	return predicate_tree_position[thread_id];
}

/* Make sure elements of thrd_inst_act_map are initialized properly when threads enter functions */
void FuncNode::init_inst_act_map(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	SnapVector<inst_act_map_t *> * thrd_inst_act_map = history->getThrdInstActMap(func_id);
	uint old_size = thrd_inst_act_map->size();

	if (thrd_inst_act_map->size() <= (uint) thread_id) {
		uint new_size = thread_id + 1;
		thrd_inst_act_map->resize(new_size);

		for (uint i = old_size;i < new_size;i++)
			(*thrd_inst_act_map)[i] = new inst_act_map_t(128);
	}
}

/* Reset elements of thrd_inst_act_map when threads exit functions */
void FuncNode::reset_inst_act_map(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	SnapVector<inst_act_map_t *> * thrd_inst_act_map = history->getThrdInstActMap(func_id);

	inst_act_map_t * map = (*thrd_inst_act_map)[thread_id];
	map->reset();
}

void FuncNode::update_inst_act_map(thread_id_t tid, ModelAction * read_act)
{
	int thread_id = id_to_int(tid);
	SnapVector<inst_act_map_t *> * thrd_inst_act_map = history->getThrdInstActMap(func_id);

	inst_act_map_t * map = (*thrd_inst_act_map)[thread_id];
	FuncInst * read_inst = get_inst(read_act);
	map->put(read_inst, read_act);
}

inst_act_map_t * FuncNode::get_inst_act_map(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	SnapVector<inst_act_map_t *> * thrd_inst_act_map = history->getThrdInstActMap(func_id);

	return (*thrd_inst_act_map)[thread_id];
}

/* Add FuncNodes that this node may follow */
void FuncNode::add_out_edge(FuncNode * other)
{
	if ( !edge_table.contains(other) ) {
		edge_table.put(other, OUT_EDGE);
		out_edges.push_back(other);
		return;
	}

	edge_type_t edge = edge_table.get(other);
	if (edge == IN_EDGE) {
		edge_table.put(other, BI_EDGE);
		out_edges.push_back(other);
	}
}

/* Compute the distance between this FuncNode and the target node.
 * Return -1 if the target node is unreachable or the actual distance
 * is greater than max_step.
 */
int FuncNode::compute_distance(FuncNode * target, int max_step)
{
	if (target == NULL)
		return -1;
	else if (target == this)
		return 0;

	SnapList<FuncNode *> queue;
	HashTable<FuncNode *, int, uintptr_t, 0> distances(128);

	queue.push_back(this);
	distances.put(this, 0);

	while (!queue.empty()) {
		FuncNode * curr = queue.front();
		queue.pop_front();
		int dist = distances.get(curr);

		if (max_step <= dist)
			return -1;

		ModelList<FuncNode *> * outEdges = curr->get_out_edges();
		mllnode<FuncNode *> * it;
		for (it = outEdges->begin();it != NULL;it = it->getNext()) {
			FuncNode * out_node = it->getVal();

			/* This node has not been visited before */
			if ( !distances.contains(out_node) ) {
				if (out_node == target)
					return dist + 1;

				queue.push_back(out_node);
				distances.put(out_node, dist + 1);
			}
		}
	}

	/* Target node is unreachable */
	return -1;
}

void FuncNode::add_failed_predicate(Predicate * pred)
{
	failed_predicates.add(pred);
}

/* Implement quick sort to sort leaves before assigning base scores */
template<typename _Tp>
static int partition(ModelVector<_Tp *> * arr, int low, int high)
{
	unsigned int pivot = (*arr)[high] -> get_depth();
	int i = low - 1;

	for (int j = low;j <= high - 1;j ++) {
		if ( (*arr)[j] -> get_depth() < pivot ) {
			i ++;
			_Tp * tmp = (*arr)[i];
			(*arr)[i] = (*arr)[j];
			(*arr)[j] = tmp;
		}
	}

	_Tp * tmp = (*arr)[i + 1];
	(*arr)[i + 1] = (*arr)[high];
	(*arr)[high] = tmp;

	return i + 1;
}

/* Implement quick sort to sort leaves before assigning base scores */
template<typename _Tp>
static void quickSort(ModelVector<_Tp *> * arr, int low, int high)
{
	if (low < high) {
		int pi = partition(arr, low, high);

		quickSort(arr, low, pi - 1);
		quickSort(arr, pi + 1, high);
	}
}

void FuncNode::assign_initial_weight()
{
	PredSetIter * it = predicate_leaves.iterator();
	leaves_tmp_storage.clear();

	while (it->hasNext()) {
		Predicate * pred = it->next();
		double weight = 100.0 / sqrt(pred->get_expl_count() + pred->get_fail_count() + 1);
		pred->set_weight(weight);
		leaves_tmp_storage.push_back(pred);
	}
	delete it;

	quickSort(&leaves_tmp_storage, 0, leaves_tmp_storage.size() - 1);

	// assign scores for internal nodes;
	while ( !leaves_tmp_storage.empty() ) {
		Predicate * leaf = leaves_tmp_storage.back();
		leaves_tmp_storage.pop_back();

		Predicate * curr = leaf->get_parent();
		while (curr != NULL) {
			if (curr->get_weight() != 0) {
				// Has been exlpored
				break;
			}

			ModelVector<Predicate *> * children = curr->get_children();
			double weight_sum = 0;
			bool has_unassigned_node = false;

			for (uint i = 0;i < children->size();i++) {
				Predicate * child = (*children)[i];

				// If a child has unassigned weight
				double weight = child->get_weight();
				if (weight == 0) {
					has_unassigned_node = true;
					break;
				} else
					weight_sum += weight;
			}

			if (!has_unassigned_node) {
				double average_weight = (double) weight_sum / (double) children->size();
				double weight = average_weight * pow(0.9, curr->get_depth());
				curr->set_weight(weight);
			} else
				break;

			curr = curr->get_parent();
		}
	}
}

void FuncNode::update_predicate_tree_weight()
{
	if (marker == 2) {
		// Predicate tree is initially built
		assign_initial_weight();
		return;
	}

	weight_debug_vec.clear();

	PredSetIter * it = failed_predicates.iterator();
	while (it->hasNext()) {
		Predicate * pred = it->next();
		leaves_tmp_storage.push_back(pred);
	}
	delete it;
	failed_predicates.reset();

	quickSort(&leaves_tmp_storage, 0, leaves_tmp_storage.size() - 1);
	for (uint i = 0;i < leaves_tmp_storage.size();i++) {
		Predicate * pred = leaves_tmp_storage[i];
		double weight = 100.0 / sqrt(pred->get_expl_count() + pred->get_fail_count() + 1);
		pred->set_weight(weight);
	}

	// Update weights in internal nodes
	while ( !leaves_tmp_storage.empty() ) {
		Predicate * leaf = leaves_tmp_storage.back();
		leaves_tmp_storage.pop_back();

		Predicate * curr = leaf->get_parent();
		while (curr != NULL) {
			ModelVector<Predicate *> * children = curr->get_children();
			double weight_sum = 0;
			bool has_unassigned_node = false;

			for (uint i = 0;i < children->size();i++) {
				Predicate * child = (*children)[i];

				double weight = child->get_weight();
				if (weight != 0)
					weight_sum += weight;
				else if ( predicate_leaves.contains(child) ) {
					// If this child is a leaf
					double weight = 100.0 / sqrt(child->get_expl_count() + 1);
					child->set_weight(weight);
					weight_sum += weight;
				} else {
					has_unassigned_node = true;
					weight_debug_vec.push_back(child);	// For debugging purpose
					break;
				}
			}

			if (!has_unassigned_node) {
				double average_weight = (double) weight_sum / (double) children->size();
				double weight = average_weight * pow(0.9, curr->get_depth());
				curr->set_weight(weight);
			} else
				break;

			curr = curr->get_parent();
		}
	}

	for (uint i = 0;i < weight_debug_vec.size();i++) {
		Predicate * tmp = weight_debug_vec[i];
		ASSERT( tmp->get_weight() != 0 );
	}
}

void FuncNode::print_predicate_tree()
{
	model_print("digraph function_%s {\n", func_name);
	predicate_tree_entry->print_pred_subtree();
	predicate_tree_exit->print_predicate();
	model_print("}\n");	// end of graph
}
