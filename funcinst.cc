#include "funcinst.h"

FuncInst::FuncInst(ModelAction *act) :
	collisions()
{
	ASSERT(act);
	this->position = act->get_position();
	this->location = act->get_location();
	this->type = act->get_type();
}

bool FuncInst::add_pred(FuncInst * other) {
	func_inst_list_mt::iterator it;
	for (it = predecessors.begin(); it != predecessors.end(); it++) {
		FuncInst * inst = *it;
		if (inst == other)
			return false;
	}

	predecessors.push_back(other);
	return true;
}

bool FuncInst::add_succ(FuncInst * other) {
	func_inst_list_mt::iterator it;
	for (it = successors.begin(); it != successors.end(); it++) {
		FuncInst * inst = *it;
		if ( inst == other )
			return false;
	}

	successors.push_back(other);
	return true;
}

FuncInst * FuncInst::search_in_collision(ModelAction *act) {
	action_type type = act->get_type();

	func_inst_list_mt::iterator it;
	for (it = collisions.begin(); it != collisions.end(); it++) {
		FuncInst * inst = *it;
		if ( inst->get_type() == type )
			return inst;
	}
	return NULL;
}