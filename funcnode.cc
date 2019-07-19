#include "funcnode.h"

FuncNode::FuncNode() :
	func_inst_map(),
	inst_list(),
	entry_insts()
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

/* Store the values read by atomic read actions into loc_thrd_read_map */
void FuncNode::store_read(ModelAction * act, uint32_t tid)
{
	ASSERT(act);

	void * location = act->get_location();
	uint64_t read_from_val = act->get_reads_from_value();

	ModelVector<uint64_t> * read_vals = loc_thrd_read_map.get(location);
	if (read_vals == NULL) {
		read_vals = new ModelVector<uint64_t>();
		loc_thrd_read_map.put(location, read_vals);
	}

	if (read_vals->size() <= tid) {
		read_vals->resize(tid + 1);
	}
	read_vals->at(tid) = read_from_val;

	/* Store keys of loc_thrd_read_map into read_locations */
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

/* @param tid thread id
 * Print the values read by the last read actions per memory location
 */
void FuncNode::print_last_read(uint32_t tid)
{
	ModelList<void *>::iterator it;
	for (it = read_locations.begin(); it != read_locations.end(); it++) {
		ModelVector<uint64_t> * read_vals = loc_thrd_read_map.get(*it);
		if (read_vals->size() <= tid)
			break;

		int64_t read_val = read_vals->at(tid);
		model_print("last read of thread %d at %p: 0x%x\n", tid, *it, read_val);
	}
}
