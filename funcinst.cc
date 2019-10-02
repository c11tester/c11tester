#include "funcinst.h"
#include "model.h"

FuncInst::FuncInst(ModelAction *act, FuncNode *func_node) :
	single_location(true),
	execution_number(model->get_execution_number())
{
	ASSERT(act);
	ASSERT(func_node);
	this->position = act->get_position();
	this->location = act->get_location();
	this->type = act->get_type();
	this->order = act->get_mo();
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
	mllnode<FuncInst*> * it;
	for (it = predecessors.begin();it != NULL;it=it->getNext()) {
		FuncInst * inst = it->getVal();
		if (inst == other)
			return false;
	}

	predecessors.push_back(other);
	return true;
}

bool FuncInst::add_succ(FuncInst * other)
{
	mllnode<FuncInst*>* it;
	for (it = successors.begin();it != NULL;it=it->getNext()) {
		FuncInst * inst = it->getVal();
		if ( inst == other )
			return false;
	}

	successors.push_back(other);
	return true;
}

/*
FuncInst * FuncInst::search_in_collision(ModelAction *act)
{
	action_type type = act->get_type();

	mllnode<FuncInst*> * it;
	for (it = collisions.begin(); it != NULL; it = it->getNext()) {
		FuncInst * inst = it->getVal();
		if (inst->get_type() == type)
			return inst;
	}
	return NULL;
}
*/

/* Note: is_read() is equivalent to ModelAction::is_read() */
bool FuncInst::is_read() const
{
	return type == ATOMIC_READ || type == ATOMIC_RMWR || type == ATOMIC_RMWRCAS || type == ATOMIC_RMW;
}

/* Note: because of action type conversion in ModelExecution
 * is_write() <==> pure writes (excluding rmw) */
bool FuncInst::is_write() const
{
	return type == ATOMIC_WRITE || type == ATOMIC_RMW || type == ATOMIC_INIT || type == ATOMIC_UNINIT || type == NONATOMIC_WRITE;
}

void FuncInst::print()
{
	model_print("func inst - pos: %s, loc: %p, type: %d,\n", position, location, type);
}
