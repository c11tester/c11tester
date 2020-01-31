/** @file action.h
 *  @brief Models actions taken by threads.
 */

#ifndef __ACTION_H__
#define __ACTION_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "memoryorder.h"
#include "modeltypes.h"
#include "mypthread.h"
#include "classlist.h"

namespace cdsc {
class mutex;
}

using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

/**
 * @brief A recognizable don't-care value for use in the ModelAction::value
 * field
 *
 * Note that this value can be legitimately used by a program, and hence by
 * iteself does not indicate no value.
 */
#define VALUE_NONE 0xdeadbeef
#define WRITE_REFERENCED ((void *)0x1)

/**
 * @brief The "location" at which a fence occurs
 *
 * We need a non-zero memory location to associate with fences, since our hash
 * tables don't handle NULL-pointer keys. HACK: Hopefully this doesn't collide
 * with any legitimate memory locations.
 */
#define FENCE_LOCATION ((void *)0x7)

/** @brief Represents an action type, identifying one of several types of
 * ModelAction */
typedef enum action_type {
	THREAD_CREATE,	// < A thread creation action
	THREAD_START,	// < First action in each thread
	THREAD_YIELD,	// < A thread yield action
	THREAD_JOIN,	// < A thread join action
	THREAD_FINISH,	// < A thread completion action
	THREADONLY_FINISH,	// < A thread completion action
	THREAD_SLEEP,	// < A sleep operation

	PTHREAD_CREATE,	// < A pthread creation action
	PTHREAD_JOIN,	// < A pthread join action

	NONATOMIC_WRITE,	// < Represents a non-atomic store
	ATOMIC_INIT,	// < Initialization of an atomic object (e.g., atomic_init())
	ATOMIC_WRITE,	// < An atomic write action
	ATOMIC_RMW,	// < The write part of an atomic RMW action
	ATOMIC_READ,	// < An atomic read action
	ATOMIC_RMWR,	// < The read part of an atomic RMW action
	ATOMIC_RMWRCAS,	// < The read part of an atomic RMW action
	ATOMIC_RMWC,	// < Convert an atomic RMW action into a READ

	ATOMIC_FENCE,	// < A fence action
	ATOMIC_LOCK,	// < A lock action
	ATOMIC_TRYLOCK,	// < A trylock action
	ATOMIC_UNLOCK,	// < An unlock action

	ATOMIC_NOTIFY_ONE,	// < A notify_one action
	ATOMIC_NOTIFY_ALL,	// < A notify all action
	ATOMIC_WAIT,	// < A wait action
	ATOMIC_TIMEDWAIT,	// < A timed wait action
	ATOMIC_ANNOTATION,	// < An annotation action to pass information to a trace analysis
	READY_FREE,	// < Write is ready to be freed
	ATOMIC_NOP	// < Placeholder
} action_type_t;


/**
 * @brief Represents a single atomic action
 *
 * A ModelAction is always allocated as non-snapshotting, because it is used in
 * multiple executions during backtracking. Except for non-atomic write
 * ModelActions, each action is assigned a unique sequence
 * number.
 */
class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, uint64_t value = VALUE_NONE, Thread *thread = NULL);
	ModelAction(action_type_t type, memory_order order, void *loc, uint64_t value, int size);
	ModelAction(action_type_t type, const char * position, memory_order order, void *loc, uint64_t value, int size);
	ModelAction(action_type_t type, memory_order order, uint64_t value, uint64_t time);
	ModelAction(action_type_t type, const char * position, memory_order order, void *loc, uint64_t value = VALUE_NONE, Thread *thread = NULL);
	~ModelAction();
	void print() const;

	thread_id_t get_tid() const { return tid; }
	action_type get_type() const { return type; }
	void set_type(action_type _type) { type = _type; }
	action_type get_original_type() const { return original_type; }
	void set_original_type(action_type _type) { original_type = _type; }
	void set_free() { type = READY_FREE; }
	memory_order get_mo() const { return order; }
	memory_order get_original_mo() const { return original_order; }
	void set_mo(memory_order order) { this->order = order; }
	void * get_location() const { return location; }
	const char * get_position() const { return position; }
	modelclock_t get_seq_number() const { return seq_number; }
	uint64_t get_value() const { return value; }
	uint64_t get_reads_from_value() const;
	uint64_t get_write_value() const;
	uint64_t get_return_value() const;
	ModelAction * get_reads_from() const { return reads_from; }
	uint64_t get_time() const {return time;}
	cdsc::mutex * get_mutex() const;
	bool get_swap_flag() const { return swap_flag; }

	void set_read_from(ModelAction *act);

	/** Store the most recent fence-release from the same thread
	 *  @param fence The fence-release that occured prior to this */
	void set_last_fence_release(const ModelAction *fence) { last_fence_release = fence; }
	/** @return The most recent fence-release from the same thread */
	const ModelAction * get_last_fence_release() const { return last_fence_release; }

	void copy_from_new(ModelAction *newaction);
	void set_seq_number(modelclock_t num);
	void reset_seq_number();
	void set_try_lock(bool obtainedlock);
	bool is_thread_start() const;
	bool is_thread_join() const;
	bool is_mutex_op() const;
	bool is_lock() const;
	bool is_sleep() const;
	bool is_trylock() const;
	bool is_unlock() const;
	bool is_wait() const;
	bool is_create() const;
	bool is_notify() const;
	bool is_notify_one() const;
	bool is_success_lock() const;
	bool is_failed_trylock() const;
	bool is_atomic_var() const;
	bool is_read() const;
	bool is_write() const;
	bool is_free() const;
	bool is_yield() const;
	bool could_be_write() const;
	bool is_rmwr() const;
	bool is_rmwrcas() const;
	bool is_rmwc() const;
	bool is_rmw() const;
	bool is_fence() const;
	bool is_initialization() const;
	bool is_annotation() const;
	bool is_relaxed() const;
	bool is_acquire() const;
	bool is_release() const;
	bool is_seqcst() const;
	bool same_var(const ModelAction *act) const;
	bool same_thread(const ModelAction *act) const;
	bool is_conflicting_lock(const ModelAction *act) const;
	bool could_synchronize_with(const ModelAction *act) const;
	int getSize() const;
	Thread * get_thread_operand() const;
	void create_cv(const ModelAction *parent = NULL);
	ClockVector * get_cv() const { return cv; }
	ClockVector * get_rfcv() const { return rf_cv; }
	void set_rfcv(ClockVector * rfcv) { rf_cv = rfcv; }
	bool synchronize_with(const ModelAction *act);

	bool has_synchronized_with(const ModelAction *act) const;
	bool happens_before(const ModelAction *act) const;

	inline bool operator <(const ModelAction& act) const {
		return get_seq_number() < act.get_seq_number();
	}
	inline bool operator >(const ModelAction& act) const {
		return get_seq_number() > act.get_seq_number();
	}

	void process_rmw(ModelAction * act);
	void copy_typeandorder(ModelAction * act);
	unsigned int hash() const;
	bool equals(const ModelAction *x) const { return this == x; }
	void set_value(uint64_t val) { value = val; }

	void use_original_type();

	/* to accomodate pthread create and join */
	Thread * thread_operand;
	void set_thread_operand(Thread *th) { thread_operand = th; }
	void setTraceRef(sllnode<ModelAction *> *ref) { trace_ref = ref; }
	void setThrdMapRef(sllnode<ModelAction *> *ref) { thrdmap_ref = ref; }
	void setActionRef(sllnode<ModelAction *> *ref) { action_ref = ref; }
	sllnode<ModelAction *> * getTraceRef() { return trace_ref; }
	sllnode<ModelAction *> * getThrdMapRef() { return thrdmap_ref; }
	sllnode<ModelAction *> * getActionRef() { return action_ref; }

	void incr_func_ref_count() { func_ref_count++; }
	void decr_func_ref_count() { if (func_ref_count > 0) func_ref_count--; }
	uint32_t get_func_ref_count() { return func_ref_count; }

	SNAPSHOTALLOC
private:
	const char * get_type_str() const;
	const char * get_mo_str() const;

	/** @brief A pointer to the memory location for this action. */
	void *location;

	/** @brief A pointer to the source line for this atomic action. */
	const char * position;

	union {
		/**
		 * @brief The store that this action reads from
		 *
		 * Only valid for reads
		 */
		ModelAction *reads_from;
		int size;
		uint64_t time;	//used for sleep
	};

	/** @brief The last fence release from the same thread */
	const ModelAction *last_fence_release;

	/**
	 * @brief The clock vector for this operation
	 *
	 * Technically, this is only needed for potentially synchronizing
	 * (e.g., non-relaxed) operations, but it is very handy to have these
	 * vectors for all operations.
	 */
	ClockVector *cv;
	ClockVector *rf_cv;
	sllnode<ModelAction *> * trace_ref;
	sllnode<ModelAction *> * thrdmap_ref;
	sllnode<ModelAction *> * action_ref;

	/** @brief Number of read actions that are reading from this store */
	uint32_t func_ref_count;

	/** @brief The value written (for write or RMW; undefined for read) */
	uint64_t value;

	/** @brief Type of action (read, write, RMW, fence, thread create, etc.) */
	action_type type;

	/** @brief The original type of action (read, write, RMW) before it was 
	 * set as READY_FREE or weaken from a RMW to a write */
	action_type original_type;

	/** @brief Indicate whether the action type and the original action type 
	 *  has been swapped. */
	bool swap_flag;

	/** @brief The memory order for this operation. */
	memory_order order;

	/** @brief The original memory order parameter for this operation. */
	memory_order original_order;

	/** @brief The thread id that performed this action. */
	thread_id_t tid;

	/**
	 * @brief The sequence number of this action
	 *
	 * Except for non atomic write actions, this number should be unique and
	 * should represent the action's position in the execution order.
	 */
	modelclock_t seq_number;
};

#endif	/* __ACTION_H__ */
