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
			if (func_inst->is_read())
				group_reads_by_loc(func_inst);

			return func_inst;
		}

		return inst;
	}

	FuncInst * func_inst = new FuncInst(act, this);
	func_inst_map.put(position, func_inst);
	inst_list.push_back(func_inst);

	if (func_inst->is_read())
		group_reads_by_loc(func_inst);

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

/* group atomic read actions by memory location */
void FuncNode::group_reads_by_loc(FuncInst * inst)
{
	ASSERT(inst);
	if ( !inst->is_read() )
		return;

	void * location = inst->get_location();

	func_inst_list_mt * reads;
	if ( !reads_by_loc.contains(location) ) {
		reads = new func_inst_list_mt();
		reads->push_back(inst);
		reads_by_loc.put(location, reads);
		return;
	}

	reads = reads_by_loc.get(location);
	func_inst_list_mt::iterator it;
	for (it = reads->begin(); it != reads->end(); it++) {
		if (inst == *it)
			return;
	}

	reads->push_back(inst);
}
