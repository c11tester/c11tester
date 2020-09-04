#include <stdio.h>
#include <algorithm>
#include <new>
#include <stdarg.h>
#include <string.h>
#include <cstdlib>

#include "model.h"
#include "action.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"
#include "datarace.h"
#include "threads-model.h"
#include "output.h"
#include "traceanalysis.h"
#include "execution.h"
#include "history.h"
#include "bugmessage.h"
#include "params.h"
#include "plugins.h"

ModelChecker *model = NULL;

void placeholder(void *) {
	ASSERT(0);
}

#include <signal.h>

#define SIGSTACKSIZE 65536
static void mprot_handle_pf(int sig, siginfo_t *si, void *unused)
{
	model_print("Segmentation fault at %p\n", si->si_addr);
	model_print("For debugging, place breakpoint at: %s:%d\n",
							__FILE__, __LINE__);
	print_trace();	// Trace printing may cause dynamic memory allocation
	while(1)
		;
}

void install_handler() {
	stack_t ss;
	ss.ss_sp = model_malloc(SIGSTACKSIZE);
	ss.ss_size = SIGSTACKSIZE;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = mprot_handle_pf;

	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction(SIGSEGV)");
		exit(EXIT_FAILURE);
	}

}

/** @brief Constructor */
ModelChecker::ModelChecker() :
	/* Initialize default scheduler */
	params(),
	scheduler(new Scheduler()),
	history(new ModelHistory()),
	execution(new ModelExecution(this, scheduler)),
	execution_number(1),
	curr_thread_num(1),
	trace_analyses(),
	inspect_plugin(NULL)
{
	model_print("C11Tester\n"
							"Copyright (c) 2013 and 2019 Regents of the University of California. All rights reserved.\n"
							"Distributed under the GPLv2\n"
							"Written by Weiyu Luo, Brian Norris, and Brian Demsky\n\n");
	memset(&stats,0,sizeof(struct execution_stats));
	init_thread = new Thread(execution->get_next_id(), (thrd_t *) model_malloc(sizeof(thrd_t)), &placeholder, NULL, NULL);
#ifdef TLS
	init_thread->setTLS((char *)get_tls_addr());
#endif
	execution->add_thread(init_thread);
	scheduler->set_current_thread(init_thread);
	register_plugins();
	execution->setParams(&params);
	param_defaults(&params);
	parse_options(&params);
	initRaceDetector();
	/* Configure output redirection for the model-checker */
	install_handler();
}

/** @brief Destructor */
ModelChecker::~ModelChecker()
{
	delete scheduler;
}

/** Method to set parameters */
model_params * ModelChecker::getParams() {
	return &params;
}

/**
 * Restores user program to initial state and resets all model-checker data
 * structures.
 */
void ModelChecker::reset_to_initial_state()
{

	/**
	 * FIXME: if we utilize partial rollback, we will need to free only
	 * those pending actions which were NOT pending before the rollback
	 * point
	 */
	for (unsigned int i = 0;i < get_num_threads();i++)
		delete get_thread(int_to_id(i))->get_pending();

	snapshot_roll_back(snapshot);
}

/** @return the number of user threads created during this execution */
unsigned int ModelChecker::get_num_threads() const
{
	return execution->get_num_threads();
}

/**
 * Must be called from user-thread context (e.g., through the global
 * thread_current() interface)
 *
 * @return The currently executing Thread.
 */
Thread * ModelChecker::get_current_thread() const
{
	return scheduler->get_current_thread();
}

/**
 * Must be called from user-thread context (e.g., through the global
 * thread_current_id() interface)
 *
 * @return The id of the currently executing Thread.
 */
thread_id_t ModelChecker::get_current_thread_id() const
{
	ASSERT(int_to_id(curr_thread_num) == get_current_thread()->get_id());
	return int_to_id(curr_thread_num);
}

/**
 * @brief Choose the next thread to execute.
 *
 * This function chooses the next thread that should execute. It can enforce
 * execution replay/backtracking or, if the model-checker has no preference
 * regarding the next thread (i.e., when exploring a new execution ordering),
 * we defer to the scheduler.
 *
 * @return The next chosen thread to run, if any exist. Or else if the current
 * execution should terminate, return NULL.
 */
Thread * ModelChecker::get_next_thread()
{

	/*
	 * Have we completed exploring the preselected path? Then let the
	 * scheduler decide
	 */
	return scheduler->select_next_thread();
}

/**
 * @brief Assert a bug in the executing program.
 *
 * Use this function to assert any sort of bug in the user program. If the
 * current trace is feasible (actually, a prefix of some feasible execution),
 * then this execution will be aborted, printing the appropriate message. If
 * the current trace is not yet feasible, the error message will be stashed and
 * printed if the execution ever becomes feasible.
 *
 * @param msg Descriptive message for the bug (do not include newline char)
 * @return True if bug is immediately-feasible
 */
void ModelChecker::assert_bug(const char *msg, ...)
{
	char str[800];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(str, sizeof(str), msg, ap);
	va_end(ap);

	execution->assert_bug(str);
}

/**
 * @brief Assert a bug in the executing program, asserted by a user thread
 * @see ModelChecker::assert_bug
 * @param msg Descriptive message for the bug (do not include newline char)
 */
void ModelChecker::assert_user_bug(const char *msg)
{
	/* If feasible bug, bail out now */
	assert_bug(msg);
	switch_thread(NULL);
}

/** @brief Print bug report listing for this execution (if any bugs exist) */
void ModelChecker::print_bugs() const
{
	SnapVector<bug_message *> *bugs = execution->get_bugs();

	model_print("Bug report: %zu bug%s detected\n",
							bugs->size(),
							bugs->size() > 1 ? "s" : "");
	for (unsigned int i = 0;i < bugs->size();i++)
		(*bugs)[i] -> print();
}

/**
 * @brief Record end-of-execution stats
 *
 * Must be run when exiting an execution. Records various stats.
 * @see struct execution_stats
 */
void ModelChecker::record_stats()
{
	stats.num_total ++;
	if (execution->have_bug_reports())
		stats.num_buggy_executions ++;
	else if (execution->is_complete_execution())
		stats.num_complete ++;
	else {
		//All threads are sleeping
		/**
		 * @todo We can violate this ASSERT() when fairness/sleep sets
		 * conflict to cause an execution to terminate, e.g. with:
		 * Scheduler: [0: disabled][1: disabled][2: sleep][3: current, enabled]
		 */
		//ASSERT(scheduler->all_threads_sleeping());
	}
}

/** @brief Print execution stats */
void ModelChecker::print_stats() const
{
	model_print("Number of complete, bug-free executions: %d\n", stats.num_complete);
	model_print("Number of buggy executions: %d\n", stats.num_buggy_executions);
	model_print("Total executions: %d\n", stats.num_total);
}

/**
 * @brief End-of-exeuction print
 * @param printbugs Should any existing bugs be printed?
 */
void ModelChecker::print_execution(bool printbugs) const
{
	model_print("Program output from execution %d:\n",
							get_execution_number());
	print_program_output();

	if (params.verbose >= 3) {
		print_stats();
	}

	/* Don't print invalid bugs */
	if (printbugs && execution->have_bug_reports()) {
		model_print("\n");
		print_bugs();
	}

	model_print("\n");
	execution->print_summary();
}

/**
 * Queries the model-checker for more executions to explore and, if one
 * exists, resets the model-checker state to execute a new execution.
 *
 * @return If there are more executions to explore, return true. Otherwise,
 * return false.
 */
void ModelChecker::finish_execution(bool more_executions)
{
	DBG();
	/* Is this execution a feasible execution that's worth bug-checking? */
	bool complete = (execution->is_complete_execution() ||
									 execution->have_bug_reports());

	/* End-of-execution bug checks */
	if (complete) {
		if (execution->is_deadlocked())
			assert_bug("Deadlock detected");

		run_trace_analyses();
	}

	record_stats();
	/* Output */
	if ( (complete && params.verbose) || params.verbose>1 || (complete && execution->have_bug_reports()))
		print_execution(complete);
	else
		clear_program_output();

	execution_number ++;
	history->set_new_exec_flag();

	if (more_executions)
		reset_to_initial_state();
}

/** @brief Run trace analyses on complete trace */
void ModelChecker::run_trace_analyses() {
	for (unsigned int i = 0;i < trace_analyses.size();i ++)
		trace_analyses[i] -> analyze(execution->get_action_trace());
}

/**
 * @brief Get a Thread reference by its ID
 * @param tid The Thread's ID
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(thread_id_t tid) const
{
	return execution->get_thread(tid);
}

/**
 * @brief Get a reference to the Thread in which a ModelAction was executed
 * @param act The ModelAction
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(const ModelAction *act) const
{
	return execution->get_thread(act);
}

void ModelChecker::startRunExecution(Thread *old) {
	while (true) {
		if (params.traceminsize != 0 &&
				execution->get_curr_seq_num() > checkfree) {
			checkfree += params.checkthreshold;
			execution->collectActions();
		}

		curr_thread_num = 1;
		Thread *thr = getNextThread(old);
		if (thr != nullptr) {
			scheduler->set_current_thread(thr);

			if (Thread::swap(old, thr) < 0) {
				perror("swap threads");
				exit(EXIT_FAILURE);
			}
			return;
		}

		if (!handleChosenThread(old)) {
			return;
		}
	}
}

Thread* ModelChecker::getNextThread(Thread *old)
{
	Thread *nextThread = nullptr;
	for (unsigned int i = curr_thread_num;i < get_num_threads();i++) {
		thread_id_t tid = int_to_id(i);
		Thread *thr = get_thread(tid);

		if (!thr->is_complete()) {
			if (!thr->get_pending()) {
				curr_thread_num = i;
				nextThread = thr;
				break;
			}
		} else if (thr != old && !thr->is_freed()) {
			thr->freeResources();
		}

		ModelAction *act = thr->get_pending();
		if (act && execution->is_enabled(tid)){
			/* Don't schedule threads which should be disabled */
			if (!execution->check_action_enabled(act)) {
				scheduler->sleep(thr);
			}

			/* Allow pending relaxed/release stores or thread actions to perform first */
			else if (!chosen_thread) {
				if (act->is_write()) {
					std::memory_order order = act->get_mo();
					if (order == std::memory_order_relaxed || \
							order == std::memory_order_release) {
						chosen_thread = thr;
					}
				} else if (act->get_type() == THREAD_CREATE || \
									 act->get_type() == PTHREAD_CREATE || \
									 act->get_type() == THREAD_START || \
									 act->get_type() == THREAD_FINISH) {
					chosen_thread = thr;
				}
			}
		}
	}
	return nextThread;
}

/* Swap back to system_context and terminate this execution */
void ModelChecker::finishRunExecution(Thread *old)
{
	scheduler->set_current_thread(NULL);

	/** Reset curr_thread_num to initial value for next execution. */
	curr_thread_num = 1;

	/** If we have more executions, we won't make it past this call. */
	finish_execution(execution_number < params.maxexecutions);


	/** We finished the final execution.  Print stuff and exit. */
	model_print("******* Model-checking complete: *******\n");
	print_stats();

	/* Have the trace analyses dump their output. */
	for (unsigned int i = 0;i < trace_analyses.size();i++)
		trace_analyses[i]->finish();

	/* unlink tmp file created by last child process */
	char filename[256];
	snprintf_(filename, sizeof(filename), "C11FuzzerTmp%d", getpid());
	unlink(filename);

	/* Exit. */
	_Exit(0);
}

uint64_t ModelChecker::switch_thread(ModelAction *act)
{
	if (modellock) {
		static bool fork_message_printed = false;

		if (!fork_message_printed) {
			model_print("Fork handler or dead thread trying to call into model checker...\n");
			fork_message_printed = true;
		}
		delete act;
		return 0;
	}
	DBG();
	Thread *old = thread_current();
	old->set_state(THREAD_READY);

	ASSERT(!old->get_pending());

	if (inspect_plugin != NULL) {
		inspect_plugin->inspectModelAction(act);
	}

	old->set_pending(act);

	if (old->is_waiting_on(old))
		assert_bug("Deadlock detected (thread %u)", curr_thread_num);

	Thread* next = getNextThread(old);
	if (next != nullptr) {
		scheduler->set_current_thread(next);
		if (Thread::swap(old, next) < 0) {
			perror("swap threads");
			exit(EXIT_FAILURE);
		}
	} else {
		if (handleChosenThread(old)) {
			startRunExecution(old);
		}
	}
	return old->get_return_value();
}

bool ModelChecker::handleChosenThread(Thread *old)
{
	if (execution->has_asserted()) {
		finishRunExecution(old);
		return false;
	}
	if (!chosen_thread) {
		chosen_thread = get_next_thread();
	}
	if (!chosen_thread) {
		finishRunExecution(old);
		return false;
	}
	if (chosen_thread->just_woken_up()) {
		chosen_thread->set_wakeup_state(false);
		chosen_thread->set_pending(NULL);
		chosen_thread = NULL;
		// Allow this thread to stash the next pending action
		return true;
	}

	// Consume the next action for a Thread
	ModelAction *curr = chosen_thread->get_pending();
	chosen_thread->set_pending(NULL);
	chosen_thread = execution->take_step(curr);

	if (should_terminate_execution()) {
		finishRunExecution(old);
		return false;
	} else {
		return true;
	}
}

void ModelChecker::startChecker() {
	startExecution();
	//Need to initial random number generator state to avoid resets on rollback
	initstate(423121, random_state, sizeof(random_state));

	snapshot = take_snapshot();

	//reset random number generator state
	setstate(random_state);

	install_trace_analyses(get_execution());
	redirect_output();
	initMainThread();
}

bool ModelChecker::should_terminate_execution()
{
	if (execution->have_bug_reports()) {
		execution->set_assert();
		return true;
	} else if (execution->isFinished()) {
		return true;
	}
	return false;
}
