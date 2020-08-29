#include "action.h"
#include "history.h"
#include "funcnode.h"
#include "funcinst.h"
#include "predicate.h"
#include "concretepredicate.h"

#include "model.h"
#include "execution.h"
#include "newfuzzer.h"
#include <cmath>

FuncNode::FuncNode(ModelHistory * history) :
	func_id(0),
	func_name(NULL),
	history(history),
	inst_counter(1),
	marker(1),
	exit_count(0),
	thrd_markers(),
	thrd_recursion_depth(),
	func_inst_map(),
	inst_list(),
	entry_insts(),
	thrd_inst_pred_maps(),
	thrd_inst_id_maps(),
	thrd_loc_inst_maps(),
	likely_null_set(),
	thrd_predicate_tree_position(),
	thrd_predicate_trace(),
	edge_table(32),
	out_edges()
{
	predicate_tree_entry = new Predicate(NULL, true);
	predicate_tree_entry->add_predicate_expr(NOPREDICATE, NULL, true);

	predicate_tree_exit = new Predicate(NULL, false, true);
	predicate_tree_exit->set_depth(MAX_DEPTH);

	/* Snapshot data structures below */
	read_locations = new loc_set_t();
	write_locations = new loc_set_t();
	val_loc_map = new HashTable<uint64_t, loc_set_t *, uint64_t, 0, snapshot_malloc, snapshot_calloc, snapshot_free, int64_hash>();
	loc_may_equal_map = new HashTable<void *, loc_set_t *, uintptr_t, 0>();

	//values_may_read_from = new value_set_t();
}

/* Reallocate snapshotted memories when new executions start */
void FuncNode::set_new_exec_flag()
{
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

void FuncNode::function_entry_handler(thread_id_t tid)
{
	init_marker(tid);
	init_local_maps(tid);
	init_predicate_tree_data_structure(tid);
}

void FuncNode::function_exit_handler(thread_id_t tid)
{
	int thread_id = id_to_int(tid);

	reset_local_maps(tid);

	thrd_recursion_depth[thread_id]--;
	thrd_markers[thread_id]->pop_back();

	Predicate * exit_pred = get_predicate_tree_position(tid);
	if (exit_pred->get_exit() == NULL) {
		// Exit predicate is unset yet
		exit_pred->set_exit(predicate_tree_exit);
	}

	update_predicate_tree_weight(tid);
	reset_predicate_tree_data_structure(tid);

	exit_count++;
	//model_print("exit count: %d\n", exit_count);

//	print_predicate_tree();
}

/**
 * @brief Convert ModelAdtion list to FuncInst list
 * @param act_list A list of ModelActions
 */
void FuncNode::update_tree(ModelAction * act)
{
	bool should_process = act->is_read() || act->is_write();
	if (!should_process)
		return;

	HashTable<void *, value_set_t *, uintptr_t, 0> * write_history = history->getWriteHistory();

	/* build inst_list from act_list for later processing */
//	func_inst_list_t inst_list;

	FuncInst * func_inst = get_inst(act);
	void * loc = act->get_location();

	if (func_inst == NULL)
		return;

//	inst_list.push_back(func_inst);

	if (act->is_write()) {
		if (!write_locations->contains(loc)) {
			write_locations->add(loc);
			history->update_loc_wr_func_nodes_map(loc, this);
		}
	}

	if (act->is_read()) {
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

		// Keep a has-been-zero-set record
		if ( likely_reads_from_null(act) )
			likely_null_set.put(func_inst, true);
	}

//	update_inst_tree(&inst_list); TODO

	update_predicate_tree(act);
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

void FuncNode::update_predicate_tree(ModelAction * next_act)
{
	thread_id_t tid = next_act->get_tid();
	int thread_id = id_to_int(tid);
	uint32_t this_marker = thrd_markers[thread_id]->back();
	int recursion_depth = thrd_recursion_depth[thread_id];

	loc_inst_map_t * loc_inst_map = thrd_loc_inst_maps[thread_id]->back();
	inst_pred_map_t * inst_pred_map = thrd_inst_pred_maps[thread_id]->back();
	inst_id_map_t * inst_id_map = thrd_inst_id_maps[thread_id]->back();

	Predicate * curr_pred = get_predicate_tree_position(tid);
	NewFuzzer * fuzzer = (NewFuzzer *)model->get_execution()->getFuzzer();
	Predicate * selected_branch = fuzzer->get_selected_child_branch(tid);

	bool amended;
	while (true) {
		FuncInst * next_inst = get_inst(next_act);

		Predicate * unset_predicate = NULL;
		bool branch_found = follow_branch(&curr_pred, next_inst, next_act, &unset_predicate);

		// A branch with unset predicate expression is detected
		if (!branch_found && unset_predicate != NULL) {
			amended = amend_predicate_expr(curr_pred, next_inst, next_act);
			if (amended)
				continue;
			else {
				curr_pred = unset_predicate;
				branch_found = true;
			}
		}

		// Detect loops
		if (!branch_found && inst_id_map->contains(next_inst)) {
			FuncInst * curr_inst = curr_pred->get_func_inst();
			uint32_t curr_id = inst_id_map->get(curr_inst);
			uint32_t next_id = inst_id_map->get(next_inst);

			if (curr_id >= next_id) {
				Predicate * old_pred = inst_pred_map->get(next_inst);
				Predicate * back_pred = old_pred->get_parent();

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

		if (next_act->is_write()) {
			curr_pred->set_write(true);
		}

		if (next_act->is_read()) {
			/* Only need to store the locations of read actions */
			loc_inst_map->put(next_act->get_location(), next_inst);
		}

		inst_pred_map->put(next_inst, curr_pred);
		set_predicate_tree_position(tid, curr_pred);

		if (!inst_id_map->contains(next_inst))
			inst_id_map->put(next_inst, inst_counter++);

		curr_pred->incr_expl_count();
		add_predicate_to_trace(tid, curr_pred);
		if (next_act->is_read())
			next_inst->set_associated_read(tid, recursion_depth, this_marker, next_act->get_reads_from_value());

		break;
	}

	// A check
	if (next_act->is_read()) {
//		if (selected_branch != NULL && !amended)
//			ASSERT(selected_branch == curr_pred);
	}
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
	thread_id_t tid = next_act->get_tid();

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
				to_be_compared = pred_expression->func_inst;

				last_read = get_associated_read(tid, to_be_compared);
				if (last_read == VALUE_NONE)
					predicate_correct = false;
				// ASSERT(last_read != VALUE_NONE);

				next_read = next_act->get_reads_from_value();
				equality = (last_read == next_read);
				if (equality != pred_expression->value)
					predicate_correct = false;

				break;
			case NULLITY:
				// TODO: implement likely to be null
				equality = likely_reads_from_null(next_act);
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
	int thread_id = id_to_int(next_act->get_tid());
	loc_inst_map_t * loc_inst_map = thrd_loc_inst_maps[thread_id]->back();

	if (next_inst->is_read()) {
		/* read + rmw */
		if ( loc_inst_map->contains(loc) ) {
			FuncInst * last_inst = loc_inst_map->get(loc);
			struct half_pred_expr * expression = new half_pred_expr(EQUALITY, last_inst);
			half_pred_expressions->push_back(expression);
		} else if ( next_inst->is_single_location() ) {
			loc_set_t * loc_may_equal = loc_may_equal_map->get(loc);

			if (loc_may_equal != NULL) {
				loc_set_iter * loc_it = loc_may_equal->iterator();
				while (loc_it->hasNext()) {
					void * neighbor = loc_it->next();
					if (loc_inst_map->contains(neighbor)) {
						FuncInst * last_inst = loc_inst_map->get(neighbor);

						struct half_pred_expr * expression = new half_pred_expr(EQUALITY, last_inst);
						half_pred_expressions->push_back(expression);
					}
				}

				delete loc_it;
			}
		}

		// next_inst is not single location and has been null
		bool likely_null = likely_null_set.contains(next_inst);
		if ( !next_inst->is_single_location() && likely_null ) {
			struct half_pred_expr * expression = new half_pred_expr(NULLITY, NULL);
			half_pred_expressions->push_back(expression);
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
	}

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

	bool likely_null = likely_null_set.contains(next_inst);

	// only generate NULLITY predicate when it is actually NULL.
	if ( !next_inst->is_single_location() && likely_null ) {
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

bool FuncNode::likely_reads_from_null(ModelAction * read)
{
	uint64_t read_val = read->get_reads_from_value();
	if ( (void *)(read_val && 0xffffffff) == NULL )
		return true;

	return false;
}

void FuncNode::set_predicate_tree_position(thread_id_t tid, Predicate * pred)
{
	int thread_id = id_to_int(tid);
	ModelVector<Predicate *> * stack = thrd_predicate_tree_position[thread_id];
	(*stack)[stack->size() - 1] = pred;
}

/* @return The position of a thread in a predicate tree */
Predicate * FuncNode::get_predicate_tree_position(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	return thrd_predicate_tree_position[thread_id]->back();
}

void FuncNode::add_predicate_to_trace(thread_id_t tid, Predicate * pred)
{
	int thread_id = id_to_int(tid);
	thrd_predicate_trace[thread_id]->back()->push_back(pred);
}

void FuncNode::init_marker(thread_id_t tid)
{
	marker++;

	int thread_id = id_to_int(tid);
	int old_size = thrd_markers.size();

	if (old_size < thread_id + 1) {
		thrd_markers.resize(thread_id + 1);

		for (int i = old_size;i < thread_id + 1;i++) {
			thrd_markers[i] = new ModelVector<uint32_t>();
			thrd_recursion_depth.push_back(-1);
		}
	}

	thrd_markers[thread_id]->push_back(marker);
	thrd_recursion_depth[thread_id]++;
}

uint64_t FuncNode::get_associated_read(thread_id_t tid, FuncInst * inst)
{
	int thread_id = id_to_int(tid);
	int recursion_depth = thrd_recursion_depth[thread_id];
	uint marker = thrd_markers[thread_id]->back();

	return inst->get_associated_read(tid, recursion_depth, marker);
}

/* Make sure elements of maps are initialized properly when threads enter functions */
void FuncNode::init_local_maps(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_loc_inst_maps.size();

	if (old_size < thread_id + 1) {
		int new_size = thread_id + 1;

		thrd_loc_inst_maps.resize(new_size);
		thrd_inst_id_maps.resize(new_size);
		thrd_inst_pred_maps.resize(new_size);

		for (int i = old_size;i < new_size;i++) {
			thrd_loc_inst_maps[i] = new ModelVector<loc_inst_map_t *>;
			thrd_inst_id_maps[i] = new ModelVector<inst_id_map_t *>;
			thrd_inst_pred_maps[i] = new ModelVector<inst_pred_map_t *>;
		}
	}

	ModelVector<loc_inst_map_t *> * map = thrd_loc_inst_maps[thread_id];
	int index = thrd_recursion_depth[thread_id];

	// If there are recursive calls, push more hashtables into the vector.
	if (map->size() < (uint) index + 1) {
		thrd_loc_inst_maps[thread_id]->push_back(new loc_inst_map_t(64));
		thrd_inst_id_maps[thread_id]->push_back(new inst_id_map_t(64));
		thrd_inst_pred_maps[thread_id]->push_back(new inst_pred_map_t(64));
	}

	ASSERT(map->size() == (uint) index + 1);
}

/* Reset elements of maps when threads exit functions */
void FuncNode::reset_local_maps(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int index = thrd_recursion_depth[thread_id];

	// When recursive call ends, keep only one hashtable in the vector
	if (index > 0) {
		delete thrd_loc_inst_maps[thread_id]->back();
		delete thrd_inst_id_maps[thread_id]->back();
		delete thrd_inst_pred_maps[thread_id]->back();

		thrd_loc_inst_maps[thread_id]->pop_back();
		thrd_inst_id_maps[thread_id]->pop_back();
		thrd_inst_pred_maps[thread_id]->pop_back();
	} else {
		thrd_loc_inst_maps[thread_id]->back()->reset();
		thrd_inst_id_maps[thread_id]->back()->reset();
		thrd_inst_pred_maps[thread_id]->back()->reset();
	}
}

void FuncNode::init_predicate_tree_data_structure(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_predicate_tree_position.size();

	if (old_size < thread_id + 1) {
		thrd_predicate_tree_position.resize(thread_id + 1);
		thrd_predicate_trace.resize(thread_id + 1);

		for (int i = old_size;i < thread_id + 1;i++) {
			thrd_predicate_tree_position[i] = new ModelVector<Predicate *>();
			thrd_predicate_trace[i] = new ModelVector<predicate_trace_t *>();
		}
	}

	thrd_predicate_tree_position[thread_id]->push_back(predicate_tree_entry);
	thrd_predicate_trace[thread_id]->push_back(new predicate_trace_t());
}

void FuncNode::reset_predicate_tree_data_structure(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	thrd_predicate_tree_position[thread_id]->pop_back();

	// Free memories allocated in init_predicate_tree_data_structure
	delete thrd_predicate_trace[thread_id]->back();
	thrd_predicate_trace[thread_id]->pop_back();
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

	// Be careful with memory
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

void FuncNode::update_predicate_tree_weight(thread_id_t tid)
{
	predicate_trace_t * trace = thrd_predicate_trace[id_to_int(tid)]->back();

	// Update predicate weights based on prediate trace
	for (int i = trace->size() - 1;i >= 0;i--) {
		Predicate * node = (*trace)[i];
		ModelVector<Predicate *> * children = node->get_children();

		if (children->size() == 0) {
			double weight = 100.0 / sqrt(node->get_expl_count() + node->get_fail_count() + 1);
			node->set_weight(weight);
		} else {
			double weight_sum = 0.0;
			for (uint i = 0;i < children->size();i++) {
				Predicate * child = (*children)[i];
				double weight = child->get_weight();
				weight_sum += weight;
			}

			double average_weight = (double) weight_sum / (double) children->size();
			double weight = average_weight * pow(0.9, node->get_depth());
			node->set_weight(weight);
		}
	}
}

void FuncNode::print_predicate_tree()
{
	model_print("digraph function_%s {\n", func_name);
	predicate_tree_entry->print_pred_subtree();
	predicate_tree_exit->print_predicate();
	model_print("}\n");	// end of graph
}
