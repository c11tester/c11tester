#include "waitobj.h"

WaitObj::WaitObj(thread_id_t tid) :
	tid(tid),
	waiting_for(32),
	waited_by(32),
	dist_table(32)
{}

void WaitObj::add_waiting_for(thread_id_t other, int dist)
{
	waiting_for.add(other);
	dist_table.put(other, dist);
}

void WaitObj::add_waited_by(thread_id_t other)
{
	waited_by.add(other);
}

void WaitObj::remove_waiting_for(thread_id_t other)
{
	waiting_for.remove(other);
	dist_table.remove(other);
}

void WaitObj::remove_waited_by(thread_id_t other)
{
	waited_by.remove(other);
}

int WaitObj::lookup_dist(thread_id_t other_id)
{
	if (dist_table.contains(other_id))
		return dist_table.get(other_id);

	return -1;
}

void WaitObj::reset()
{
	waiting_for.reset();
	waited_by.reset();
	dist_table.reset();
}

void WaitObj::print_waiting_for()
{
	if (waiting_for.getSize() == 0)
		return;

	model_print("thread %d is waiting for: ", tid);
	thrd_id_set_iter * it = waiting_for.iterator();

	while (it->hasNext()) {
		thread_id_t thread_id = it->next();
		model_print("%d ", thread_id);
	}
	model_print("\n");
}

void WaitObj::print_waited_by()
{
	if (waited_by.getSize() == 0)
		return;

	model_print("thread %d is waited by: ", tid);
	thrd_id_set_iter * it = waited_by.iterator();

	while (it->hasNext()) {
		thread_id_t thread_id = it->next();
		model_print("%d ", thread_id);
	}
	model_print("\n");

}
