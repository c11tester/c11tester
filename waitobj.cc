#include "waitobj.h"
#include "threads-model.h"

WaitObj::WaitObj(thread_id_t tid) :
	tid(tid),
	waiting_for(32),
	waited_by(32),
	thrd_dist_maps(),
	thrd_target_nodes()
{}

WaitObj::~WaitObj()
{
	for (uint i = 0; i < thrd_dist_maps.size(); i++)
		delete thrd_dist_maps[i];

	for (uint i = 0; i < thrd_target_nodes.size(); i++)
		delete thrd_target_nodes[i];
}

void WaitObj::add_waiting_for(thread_id_t other, FuncNode * node, int dist)
{
	waiting_for.add(other);

	dist_map_t * dist_map = getDistMap(other);
	dist_map->put(node, dist);

	node_set_t * target_nodes = getTargetNodes(other);
	target_nodes->add(node);
}

void WaitObj::add_waited_by(thread_id_t other)
{
	waited_by.add(other);
}

/**
 * Stop waiting for the thread to reach the target node
 *
 * @param other The thread to be removed
 * @param node The target node
 * @return true if "other" is removed from waiting_for set
 */
bool WaitObj::remove_waiting_for(thread_id_t other, FuncNode * node)
{
	dist_map_t * dist_map = getDistMap(other);
	dist_map->remove(node);

	node_set_t * target_nodes = getTargetNodes(other);
	target_nodes->remove(node);

	/* The thread has not nodes to reach */
	if (target_nodes->isEmpty()) {
		waiting_for.remove(other);
		return true;
	}

	return false;
}

void WaitObj::remove_waited_by(thread_id_t other)
{
	waited_by.remove(other);
}

int WaitObj::lookup_dist(thread_id_t tid, FuncNode * target)
{
	dist_map_t * map = getDistMap(tid);
	if (map->contains(target))
		return map->get(target);

	return -1;
}

dist_map_t * WaitObj::getDistMap(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_dist_maps.size();

	if (old_size <= thread_id) {
		thrd_dist_maps.resize(thread_id + 1);
		for (int i = old_size; i < thread_id + 1; i++) {
			thrd_dist_maps[i] = new dist_map_t(16);
		}
	}

	return thrd_dist_maps[thread_id];
}

node_set_t * WaitObj::getTargetNodes(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_target_nodes.size();

	if (old_size <= thread_id) {
		thrd_target_nodes.resize(thread_id + 1);
		for (int i = old_size; i < thread_id + 1; i++) {
			thrd_target_nodes[i] = new node_set_t(16);
		}
	}

	return thrd_target_nodes[thread_id];
}

void WaitObj::reset()
{
	thrd_id_set_iter * iter = waiting_for.iterator();
	while (iter->hasNext()) {
		thread_id_t tid = iter->next();
		int index = id_to_int(tid);
		thrd_target_nodes[index]->reset();
		/* thrd_dist_maps are not reset because distances
		 * will be overwritten */
	}

	waiting_for.reset();
	waited_by.reset();
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
