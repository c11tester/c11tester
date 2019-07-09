#include "funcnode.h"

FuncNode::FuncNode() :
	func_insts(),
	inst_list(),
	entry_insts()
{}

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

	if ( func_insts.contains(position) ) {
		FuncInst * inst = func_insts.get(position);

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
			// model_print("collision added\n");
			
			return func_inst;
		}

		return inst;
	}

	FuncInst * func_inst = new FuncInst(act, this);
	func_insts.put(position, func_inst);

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
