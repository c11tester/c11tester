#include "funcinst.h"
#include "model.h"

FuncInst::FuncInst(ModelAction *act, FuncNode *func_node) :
	single_location(true),
	execution_number(0),
	associated_reads(),
	thrd_markers()
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

void FuncInst::set_associated_read(thread_id_t tid, int index, uint32_t marker, uint64_t read_val)
{
	int thread_id = id_to_int(tid);

	if (associated_reads.size() < (uint) thread_id + 1) {
		int old_size = associated_reads.size();
		int new_size = thread_id + 1;

		associated_reads.resize(new_size);
		thrd_markers.resize(new_size);

		for (int i = old_size;i < new_size;i++ ) {
			associated_reads[i] = new ModelVector<uint64_t>();
			thrd_markers[i] = new ModelVector<uint32_t>();
		}
	}

	ModelVector<uint64_t> * read_values = associated_reads[thread_id];
	ModelVector<uint32_t> * markers = thrd_markers[thread_id];
	if (read_values->size() < (uint) index + 1) {
		int old_size = read_values->size();

		for (int i = old_size;i < index + 1;i++) {
			read_values->push_back(VALUE_NONE);
			markers->push_back(0);
		}
	}

	(*read_values)[index] = read_val;
	(*markers)[index] = marker;
}

uint64_t FuncInst::get_associated_read(thread_id_t tid, int index, uint32_t marker)
{
	int thread_id = id_to_int(tid);

	if ( (*thrd_markers[thread_id])[index] == marker)
		return (*associated_reads[thread_id])[index];
	else
		return VALUE_NONE;
}

/* Search the FuncInst that has the same type as act in the collision list */
FuncInst * FuncInst::search_in_collision(ModelAction *act)
{
	action_type type = act->get_type();

	mllnode<FuncInst*> * it;
	for (it = collisions.begin();it != NULL;it = it->getNext()) {
		FuncInst * inst = it->getVal();
		if (inst->get_type() == type)
			return inst;
	}
	return NULL;
}

void FuncInst::add_to_collision(FuncInst * inst)
{
	collisions.push_back(inst);
}

/* Note: is_read() is equivalent to ModelAction::is_read() */
bool FuncInst::is_read() const
{
	return type == ATOMIC_READ || type == ATOMIC_RMWR || type == ATOMIC_RMWRCAS || type == ATOMIC_RMW;
}

/* Note: because of action type conversion in ModelExecution
 * is_write() <==> pure writes (excluding rmw) */
bool FuncInst::is_write() const
{
	return type == ATOMIC_WRITE || type == ATOMIC_RMW || type == ATOMIC_INIT || type == NONATOMIC_WRITE;
}

void FuncInst::print()
{
	model_print("func inst - pos: %s, loc: %p, type: %d,\n", position, location, type);
}
