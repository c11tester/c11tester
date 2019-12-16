#include "waitobj.h"
#include "threads-model.h"
#include "funcnode.h"

#define COUNTER_THRESHOLD 1000

WaitObj::WaitObj(thread_id_t tid) :
	tid(tid),
	waiting_for(32),
	waited_by(32),
	thrd_dist_maps(),
	thrd_target_nodes(),
	thrd_action_counters()
{}

WaitObj::~WaitObj()
{
	for (uint i = 0;i < thrd_dist_maps.size();i++)
		delete thrd_dist_maps[i];

	for (uint i = 0;i < thrd_target_nodes.size();i++)
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
 *         false if only a target node of "other" is removed
 */
bool WaitObj::remove_waiting_for_node(thread_id_t other, FuncNode * node)
{
	dist_map_t * dist_map = getDistMap(other);
	dist_map->remove(node);

	node_set_t * target_nodes = getTargetNodes(other);
	target_nodes->remove(node);

	/* The thread has no nodes to reach */
	if (target_nodes->isEmpty()) {
		int index = id_to_int(other);
		thrd_action_counters[index] = 0;
		waiting_for.remove(other);

		return true;
	}

	return false;
}

/* Stop waiting for the thread */
void WaitObj::remove_waiting_for(thread_id_t other)
{
	waiting_for.remove(other);

	// TODO: clear dist_map or not?
	/* dist_map_t * dist_map = getDistMap(other);
	   dist_map->reset(); */

	node_set_t * target_nodes = getTargetNodes(other);
	target_nodes->reset();

	int index = id_to_int(other);
	thrd_action_counters[index] = 0;
}

void WaitObj::remove_waited_by(thread_id_t other)
{
	waited_by.remove(other);
}

int WaitObj::lookup_dist(thread_id_t tid, FuncNode * target)
{
	dist_map_t * map = getDistMap(tid);
	node_set_t * node_set = getTargetNodes(tid);

	/* thrd_dist_maps is not reset when clear_waiting_for is called,
	 * so node_set should be checked */
	if (node_set->contains(target) && map->contains(target))
		return map->get(target);

	return -1;
}

dist_map_t * WaitObj::getDistMap(thread_id_t tid)
{
	int thread_id = id_to_int(tid);
	int old_size = thrd_dist_maps.size();

	if (old_size <= thread_id) {
		thrd_dist_maps.resize(thread_id + 1);
		for (int i = old_size;i < thread_id + 1;i++) {
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
		for (int i = old_size;i < thread_id + 1;i++) {
			thrd_target_nodes[i] = new node_set_t(16);
		}
	}

	return thrd_target_nodes[thread_id];
}

/**
 * Increment action counter for thread tid
 * @return true if the counter for tid expires
 */
bool WaitObj::incr_counter(thread_id_t tid)
{
	int thread_id = id_to_int(tid);

	/* thrd_action_counters.resize does not work here */
	while (thrd_action_counters.size() <= (uint) thread_id) {
		thrd_action_counters.push_back(0);
	}

	thrd_action_counters[thread_id]++;
	if (thrd_action_counters[thread_id] > COUNTER_THRESHOLD) {
		thrd_action_counters[thread_id] = 0;
		return true;
	}

	return false;
}

void WaitObj::clear_waiting_for()
{
	thrd_id_set_iter * iter = waiting_for.iterator();
	while (iter->hasNext()) {
		thread_id_t tid = iter->next();
		int index = id_to_int(tid);
		thrd_action_counters[index] = 0;

		/* thrd_dist_maps are not reset because distances
		 * will be overwritten when node targets are added
		 * thrd_dist_maps[index]->reset(); */

		node_set_t * target_nodes = getTargetNodes(tid);
		target_nodes->reset();
	}

	delete iter;

	waiting_for.reset();
	/* waited_by relation should be kept */
}

void WaitObj::print_waiting_for(bool verbose)
{
	if (waiting_for.getSize() == 0)
		return;

	model_print("thread %d is waiting for: ", tid);
	thrd_id_set_iter * it = waiting_for.iterator();

	while (it->hasNext()) {
		thread_id_t waiting_for_id = it->next();
		model_print("%d ", waiting_for_id);
	}
	model_print("\n");
	delete it;

	if (verbose) {
		/* Print out the distances from each thread to target nodes */
		model_print("\t");
		for (uint i = 0;i < thrd_target_nodes.size();i++) {
			dist_map_t * dist_map = getDistMap(i);
			node_set_t * node_set = getTargetNodes(i);
			node_set_iter * node_iter = node_set->iterator();

			if (!node_set->isEmpty()) {
				model_print("[thread %d](", int_to_id(i));

				while (node_iter->hasNext()) {
					FuncNode * node = node_iter->next();
					int dist = dist_map->get(node);
					model_print("node %d: %d, ", node->get_func_id(), dist);
				}
				model_print(") ");
			}

			delete node_iter;
		}
		model_print("\n");
	}
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

	delete it;
}
