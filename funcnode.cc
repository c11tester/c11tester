#include "funcnode.h"

FuncNode::FuncNode() :
	func_inst_map(),
	inst_list(),
	entry_insts(),
	thrd_read_map(),
	read_locations()
{}

/* Check whether FuncInst with the same type, position, and location 
 * as act has been added to func_inst_map or not. If so, return it;
 * if not, add it and return it.
 *
 * @return FuncInst with the same type, position, and location as act */
FuncInst * FuncNode::get_or_add_action(ModelAction *act)
{
	ASSERT(act);
	const char * position = act->get_position();

	/* Actions THREAD_CREATE, THREAD_START, THREAD_YIELD, THREAD_JOIN,
	 * THREAD_FINISH, PTHREAD_CREATE, PTHREAD_JOIN,
	 * ATOMIC_LOCK, ATOMIC_TRYLOCK, and ATOMIC_UNLOCK are not tagged with their
	 * source line numbers
	 */
	if (position == NULL)
		return NULL;

	if ( func_inst_map.contains(position) ) {
		FuncInst * inst = func_inst_map.get(position);

		if (inst->get_type() != act->get_type() ) {
			// model_print("action with a different type occurs at line number %s\n", position);
			FuncInst * func_inst = inst->search_in_collision(act);

			if (func_inst != NULL) {
				// return the FuncInst found in the collision list
				return func_inst;
			}

			func_inst = new FuncInst(act, this);
			inst->get_collisions()->push_back(func_inst);
			inst_list.push_back(func_inst);		// delete?

			return func_inst;
		}

		return inst;
	}

	FuncInst * func_inst = new FuncInst(act, this);

	func_inst_map.put(position, func_inst);
	inst_list.push_back(func_inst);

	return func_inst;
}

void FuncNode::add_entry_inst(FuncInst * inst)
{
	if (inst == NULL)
		return;

	func_inst_list_mt::iterator it;
	for (it = entry_insts.begin(); it != entry_insts.end(); it++) {
		if (inst == *it)
			return;
	}

	entry_insts.push_back(inst);
}

/* @param inst_list a list of FuncInsts; this argument comes from ModelExecution
 * Link FuncInsts in a list - add one FuncInst to another's predecessors and successors
 */
void FuncNode::link_insts(func_inst_list_t * inst_list)
{
	if (inst_list == NULL)
		return;

	func_inst_list_t::iterator it = inst_list->begin();
	func_inst_list_t::iterator prev;

	if (inst_list->size() == 0)
		return;

	/* add the first instruction to the list of entry insts */
	FuncInst * entry_inst = *it;
	add_entry_inst(entry_inst);

	it++;
	while (it != inst_list->end()) {
		prev = it;
		prev--;

		FuncInst * prev_inst = *prev;
		FuncInst * curr_inst = *it;

		prev_inst->add_succ(curr_inst);
		curr_inst->add_pred(prev_inst);

		it++;
	}
}

/* @param tid thread id
 * Store the values read by atomic read actions into thrd_read_map */
void FuncNode::store_read(ModelAction * act, uint32_t tid)
{
	ASSERT(act);

	void * location = act->get_location();
	uint64_t read_from_val = act->get_reads_from_value();

	if (thrd_read_map.size() <= tid)
		thrd_read_map.resize(tid + 1);

	read_map_t * read_map = thrd_read_map[tid];
	if (read_map == NULL) {
		read_map = new read_map_t();
		thrd_read_map[tid] = read_map;
	}

	read_map->put(location, read_from_val);

	/* Store the memory locations where atomic reads happen */
	bool push_loc = true;
	ModelList<void *>::iterator it;
	for (it = read_locations.begin(); it != read_locations.end(); it++) {
		if (location == *it) {
			push_loc = false;
			break;
		}
	}

	if (push_loc)
		read_locations.push_back(location);
}

uint64_t FuncNode::query_last_read(ModelAction * act, uint32_t tid)
{
	if (thrd_read_map.size() <= tid)
		return 0xdeadbeef;

	read_map_t * read_map = thrd_read_map[tid];
	void * location = act->get_location();

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
	ASSERT(thrd_read_map.size() > tid);
	thrd_read_map[tid]->reset();
}

/* @param tid thread id
 * Print the values read by the last read actions per memory location
 */
void FuncNode::print_last_read(uint32_t tid)
{
	ASSERT(thrd_read_map.size() > tid);
	read_map_t * read_map = thrd_read_map[tid];

	ModelList<void *>::iterator it;
	for (it = read_locations.begin(); it != read_locations.end(); it++) {
		if ( !read_map->contains(*it) )
			break;

		uint64_t read_val = read_map->get(*it);
		model_print("last read of thread %d at %p: 0x%x\n", tid, *it, read_val);
	}
}
