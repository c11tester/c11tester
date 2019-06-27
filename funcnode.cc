#include "funcnode.h"

FuncInst::FuncInst(ModelAction *act) :
	action(act)
{
	ASSERT(act);
	this->position = act->get_position();
}

FuncNode::FuncNode() :
	func_insts()
{}

