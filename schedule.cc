#include "libthreads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

void DefaultScheduler::add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->id);
	queue.push(t);
}

struct thread *DefaultScheduler::next_thread(void)
{
	if (queue.empty())
		return NULL;

	current = queue.front();
	queue.pop();

	return current;
}

struct thread *DefaultScheduler::get_current_thread(void)
{
	return current;
}