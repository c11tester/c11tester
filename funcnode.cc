#include "funcnode.h"

FuncInst::FuncInst(ModelAction *act) :
	collisions()
{
	ASSERT(act);
	this->position = act->get_position();
	this->location = act->get_location();
	this->type = act->get_type();
}

FuncNode::FuncNode() :
	func_insts(),
	inst_list(),
	entry_insts()
{}

void FuncNode::add_action(ModelAction *act)
{
	ASSERT(act);

	const char * position = act->get_position();

	/* Actions THREAD_CREATE, THREAD_START, THREAD_YIELD, THREAD_JOIN,
	 * THREAD_FINISH, PTHREAD_CREATE, PTHREAD_JOIN,
	 * ATOMIC_LOCK, ATOMIC_TRYLOCK, and ATOMIC_UNLOCK are not tagged with their
	 * source line numbers
	 */
	if (position == NULL) {
		return;
	}

	if ( func_insts.contains(position) ) {
		FuncInst * inst = func_insts.get(position);

		if (inst->get_type() != act->get_type() ) {
			model_print("action with a different type occurs at line number %s\n", position);
			FuncInst * func_inst = inst->search_in_collision(act);

			if (func_inst != NULL)
				return;

			func_inst = new FuncInst(act);
			inst->get_collisions()->push_back(func_inst);
			inst_list.push_back(func_inst);		// delete?
			model_print("collision added\n");
		}

		return;
	}

	FuncInst * func_inst = new FuncInst(act);
	func_insts.put(position, func_inst);

	inst_list.push_back(func_inst);
}
