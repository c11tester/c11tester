#include "funcinst.h"

FuncInst::FuncInst(ModelAction *act, FuncNode *func_node) :
	collisions()
{
	ASSERT(act);
	ASSERT(func_node);
	this->position = act->get_position();
	this->location = act->get_location();
	this->type = act->get_type();
	this->func_node = func_node;
}

/* @param other Preceding FuncInst in the same execution trace
 * Add other to predecessors if it has been added
 *
 * @return false: other is already in predecessors
 *         true : other is added to precedessors
 */
bool FuncInst::add_pred(FuncInst * other)
{
	func_inst_list_mt::iterator it;
	for (it = predecessors.begin();it != predecessors.end();it++) {
		FuncInst * inst = *it;
		if (inst == other)
			return false;
	}

	predecessors.push_back(other);
	return true;
}

bool FuncInst::add_succ(FuncInst * other)
{
	func_inst_list_mt::iterator it;
	for (it = successors.begin();it != successors.end();it++) {
		FuncInst * inst = *it;
		if ( inst == other )
			return false;
	}

	successors.push_back(other);
	return true;
}

FuncInst * FuncInst::search_in_collision(ModelAction *act)
{
	action_type type = act->get_type();

	func_inst_list_mt::iterator it;
	for (it = collisions.begin();it != collisions.end();it++) {
		FuncInst * inst = *it;
		if ( inst->get_type() == type )
			return inst;
	}
	return NULL;
}

bool FuncInst::is_read() const
{
	return type == ATOMIC_READ || type == ATOMIC_RMWR || type == ATOMIC_RMWRCAS; /* type == ATOMIC_RMW ? */
}

bool FuncInst::is_write() const
{
        return type == ATOMIC_WRITE || type == ATOMIC_RMW || type == ATOMIC_INIT || type == ATOMIC_UNINIT || type == NONATOMIC_WRITE;
}

