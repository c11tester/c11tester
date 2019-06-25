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

typedef SnapList<ModelAction *> action_list_t;

/** @brief Model checker execution stats */
struct execution_stats {
	int num_total;	/**< @brief Total number of executions */
	int num_infeasible;	/**< @brief Number of infeasible executions */
	int num_buggy_executions;	/** @brief Number of buggy executions */
	int num_complete;	/**< @brief Number of feasible, non-buggy, complete executions */
	int num_redundant;	/**< @brief Number of redundant, aborted executions */
};

/** @brief The central structure for model-checking */
class ModelChecker {
public:
	ModelChecker();
	~ModelChecker();
	void setParams(struct model_params params);
	void run();

	/** Restart the model checker, intended for pluggins. */
	void restart();

	/** Exit the model checker, intended for pluggins. */
	void exit_model_checker();

	/** @returns the context for the main model-checking system thread */
	ucontext_t * get_system_context() { return &system_context; }

	ModelExecution * get_execution() const { return execution; }
	ModelHistory * get_history() const { return history; }

	int get_execution_number() const { return execution_number; }

	Thread * get_thread(thread_id_t tid) const;
	Thread * get_thread(const ModelAction *act) const;

	Thread * get_current_thread() const;

	void switch_from_master(Thread *thread);
	uint64_t switch_to_master(ModelAction *act);

	bool assert_bug(const char *msg, ...);
	void assert_user_bug(const char *msg);

	model_params params;
	void add_trace_analysis(TraceAnalysis *a) {     trace_analyses.push_back(a); }
	void set_inspect_plugin(TraceAnalysis *a) {     inspect_plugin=a;       }

	MEMALLOC
private:
	/** Flag indicates whether to restart the model checker. */
	bool restart_flag;

	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler * const scheduler;
	NodeStack * const node_stack;
	ModelExecution *execution;
	ModelHistory *history;

	int execution_number;

	unsigned int get_num_threads() const;

	bool next_execution();
	bool should_terminate_execution();

	Thread * get_next_thread();
	void reset_to_initial_state();

	ucontext_t system_context;

	ModelVector<TraceAnalysis *> trace_analyses;

	/** @bref Implement restart. */
	void do_restart();
	/** @bref Plugin that can inspect new actions. */
	TraceAnalysis *inspect_plugin;
	/** @brief The cumulative execution stats */
	struct execution_stats stats;
	void record_stats();
	void run_trace_analyses();
	void print_bugs() const;
	void print_execution(bool printbugs) const;
	void print_stats() const;

	friend void user_main_wrapper();
};

extern ModelChecker *model;

#endif	/* __MODEL_H__ */
