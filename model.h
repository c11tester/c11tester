/** @file model.h
 *  @brief Core model checker.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "hashtable.h"
#include "config.h"
#include "modeltypes.h"
#include "stl-model.h"
#include "context.h"
#include "params.h"
#include "classlist.h"
#include "snapshot-interface.h"

/** @brief Model checker execution stats */
struct execution_stats {
	int num_total;	/**< @brief Total number of executions */
	int num_buggy_executions;	/** @brief Number of buggy executions */
	int num_complete;	/**< @brief Number of feasible, non-buggy, complete executions */
};

/** @brief The central structure for model-checking */
class ModelChecker {
public:
	ModelChecker();
	~ModelChecker();
	model_params * getParams();

	/** Exit the model checker, intended for pluggins. */
	void exit_model_checker();

	ModelExecution * get_execution() const { return execution; }
	ModelHistory * get_history() const { return history; }

	int get_execution_number() const { return execution_number; }

	Thread * get_thread(thread_id_t tid) const;
	Thread * get_thread(const ModelAction *act) const;

	Thread * get_current_thread() const;
	thread_id_t get_current_thread_id() const;

	uint64_t switch_thread(ModelAction *act);

	void assert_bug(const char *msg, ...);

	void assert_user_bug(const char *msg);

	model_params params;
	void add_trace_analysis(TraceAnalysis *a) {     trace_analyses.push_back(a); }
	void set_inspect_plugin(TraceAnalysis *a) {     inspect_plugin=a;       }
	void startChecker();
	Thread * getInitThread() {return init_thread;}
	Scheduler * getScheduler() {return scheduler;}
	MEMALLOC
private:
	/** Snapshot id we return to restart. */
	snapshot_id snapshot;

	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler * const scheduler;
	ModelHistory * history;
	ModelExecution *execution;
	Thread * init_thread;

	int execution_number;

	unsigned int curr_thread_num;
	Thread * chosen_thread;
	bool break_execution;

	void startRunExecution(Thread *old);
	void finishRunExecution(Thread *old);
	Thread * getNextThread(Thread *old);
	bool handleChosenThread(Thread *old);

	modelclock_t checkfree;

	unsigned int get_num_threads() const;

	void finish_execution(bool moreexecutions);
	bool should_terminate_execution();

	Thread * get_next_thread();
	void reset_to_initial_state();

	ModelVector<TraceAnalysis *> trace_analyses;
	char random_state[256];

	/** @bref Plugin that can inspect new actions. */
	TraceAnalysis *inspect_plugin;
	/** @brief The cumulative execution stats */
	struct execution_stats stats;
	void record_stats();
	void run_trace_analyses();
	void print_bugs() const;
	void print_execution(bool printbugs) const;
	void print_stats() const;
};

extern ModelChecker *model;
void parse_options(struct model_params *params);
void install_trace_analyses(ModelExecution *execution);

#endif	/* __MODEL_H__ */
