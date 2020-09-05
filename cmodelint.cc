#include <stdio.h>
#include <string>

#include "model.h"
#include "execution.h"
#include "action.h"
#include "history.h"
#include "cmodelint.h"
#include "snapshot-interface.h"
#include "threads-model.h"
#include "datarace.h"

memory_order orders[6] = {
	memory_order_relaxed, memory_order_consume, memory_order_acquire,
	memory_order_release, memory_order_acq_rel, memory_order_seq_cst,
};

/* ---  helper functions --- */
uint64_t model_rmwrcas_action_helper(void *obj, int atomic_index, uint64_t oldval, int size, const char *position) {
	createModelIfNotExist();
	return model->switch_thread(new ModelAction(ATOMIC_RMWRCAS, position, orders[atomic_index], obj, oldval, size));
}

uint64_t model_rmwr_action_helper(void *obj, int atomic_index, const char *position) {
	createModelIfNotExist();
	return model->switch_thread(new ModelAction(ATOMIC_RMWR, position, orders[atomic_index], obj));
}

void model_rmw_action_helper(void *obj, uint64_t val, int atomic_index, const char * position) {
	createModelIfNotExist();
	model->switch_thread(new ModelAction(ATOMIC_RMW, position, orders[atomic_index], obj, val));
}

void model_rmwc_action_helper(void *obj, int atomic_index, const char *position) {
	createModelIfNotExist();
	model->switch_thread(new ModelAction(ATOMIC_RMWC, position, orders[atomic_index], obj));
}

// cds volatile loads
#define VOLATILELOAD(size) \
	uint ## size ## _t cds_volatile_load ## size(void * obj, const char * position) { \
		createModelIfNotExist();                                                      \
		return (uint ## size ## _t)model->switch_thread(new ModelAction(ATOMIC_READ, position, memory_order_volatile_load, obj)); \
	}

VOLATILELOAD(8)
VOLATILELOAD(16)
VOLATILELOAD(32)
VOLATILELOAD(64)

// cds volatile stores
#define VOLATILESTORE(size) \
	void cds_volatile_store ## size (void * obj, uint ## size ## _t val, const char * position) { \
		createModelIfNotExist();                                                      \
		model->switch_thread(new ModelAction(ATOMIC_WRITE, position, memory_order_volatile_store, obj, (uint64_t) val)); \
		*((volatile uint ## size ## _t *)obj) = val;            \
		thread_id_t tid = thread_current_id();           \
		for(int i=0;i < size / 8;i++) {                         \
			atomraceCheckWrite(tid, (void *)(((char *)obj)+i));          \
		}                                                       \
	}

VOLATILESTORE(8)
VOLATILESTORE(16)
VOLATILESTORE(32)
VOLATILESTORE(64)

// cds atomic inits
#define CDSATOMICINT(size)                                              \
	void cds_atomic_init ## size (void * obj, uint ## size ## _t val, const char * position) { \
		createModelIfNotExist();                                                      \
		model->switch_thread(new ModelAction(ATOMIC_INIT, position, memory_order_relaxed, obj, (uint64_t) val)); \
		*((volatile uint ## size ## _t *)obj) = val;                                 \
		thread_id_t tid = thread_current_id();           \
		for(int i=0;i < size / 8;i++) {                       \
			atomraceCheckWrite(tid, (void *)(((char *)obj)+i));          \
		}                                                       \
	}

CDSATOMICINT(8)
CDSATOMICINT(16)
CDSATOMICINT(32)
CDSATOMICINT(64)

// cds atomic loads
#define CDSATOMICLOAD(size)                                             \
	uint ## size ## _t cds_atomic_load ## size(void * obj, int atomic_index, const char * position) { \
		createModelIfNotExist();                                                      \
		uint ## size ## _t val = (uint ## size ## _t)model->switch_thread( \
			new ModelAction(ATOMIC_READ, position, orders[atomic_index], obj)); \
		thread_id_t tid = thread_current_id();           \
		for(int i=0;i < size / 8;i++) {                         \
			atomraceCheckRead(tid, (void *)(((char *)obj)+i));    \
		}                                                       \
		return val; \
	}

CDSATOMICLOAD(8)
CDSATOMICLOAD(16)
CDSATOMICLOAD(32)
CDSATOMICLOAD(64)

// cds atomic stores
#define CDSATOMICSTORE(size)                                            \
	void cds_atomic_store ## size(void * obj, uint ## size ## _t val, int atomic_index, const char * position) { \
		createModelIfNotExist();                                                        \
		model->switch_thread(new ModelAction(ATOMIC_WRITE, position, orders[atomic_index], obj, (uint64_t) val)); \
		*((volatile uint ## size ## _t *)obj) = val;                     \
		thread_id_t tid = thread_current_id();           \
		for(int i=0;i < size / 8;i++) {                       \
			atomraceCheckWrite(tid, (void *)(((char *)obj)+i));          \
		}                                                       \
	}

CDSATOMICSTORE(8)
CDSATOMICSTORE(16)
CDSATOMICSTORE(32)
CDSATOMICSTORE(64)


#define _ATOMIC_RMW_(__op__, size, addr, val, atomic_index, position)            \
	({                                                                      \
		uint ## size ## _t _old = model_rmwr_action_helper(addr, atomic_index, position);   \
		uint ## size ## _t _copy = _old;                                          \
		uint ## size ## _t _val = val;                                            \
		_copy __op__ _val;                                                    \
		model_rmw_action_helper(addr, (uint64_t) _copy, atomic_index, position);        \
		*((volatile uint ## size ## _t *)addr) = _copy;                  \
		thread_id_t tid = thread_current_id();           \
		for(int i=0;i < size / 8;i++) {                       \
			atomraceCheckRead(tid,  (void *)(((char *)addr)+i));  \
			recordWrite(tid, (void *)(((char *)addr)+i));         \
		}                                                       \
		return _old;                                            \
	})

// cds atomic exchange
#define CDSATOMICEXCHANGE(size)                                         \
	uint ## size ## _t cds_atomic_exchange ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( =, size, addr, val, atomic_index, position);          \
	}

CDSATOMICEXCHANGE(8)
CDSATOMICEXCHANGE(16)
CDSATOMICEXCHANGE(32)
CDSATOMICEXCHANGE(64)

// cds atomic fetch add
#define CDSATOMICADD(size)                                              \
	uint ## size ## _t cds_atomic_fetch_add ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( +=, size, addr, val, atomic_index, position);         \
	}

CDSATOMICADD(8)
CDSATOMICADD(16)
CDSATOMICADD(32)
CDSATOMICADD(64)

// cds atomic fetch sub
#define CDSATOMICSUB(size)                                              \
	uint ## size ## _t cds_atomic_fetch_sub ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( -=, size, addr, val, atomic_index, position);         \
	}

CDSATOMICSUB(8)
CDSATOMICSUB(16)
CDSATOMICSUB(32)
CDSATOMICSUB(64)

// cds atomic fetch and
#define CDSATOMICAND(size)                                              \
	uint ## size ## _t cds_atomic_fetch_and ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( &=, size, addr, val, atomic_index, position);         \
	}

CDSATOMICAND(8)
CDSATOMICAND(16)
CDSATOMICAND(32)
CDSATOMICAND(64)

// cds atomic fetch or
#define CDSATOMICOR(size)                                               \
	uint ## size ## _t cds_atomic_fetch_or ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( |=, size, addr, val, atomic_index, position);         \
	}

CDSATOMICOR(8)
CDSATOMICOR(16)
CDSATOMICOR(32)
CDSATOMICOR(64)

// cds atomic fetch xor
#define CDSATOMICXOR(size)                                              \
	uint ## size ## _t cds_atomic_fetch_xor ## size(void* addr, uint ## size ## _t val, int atomic_index, const char * position) { \
		_ATOMIC_RMW_( ^=, size, addr, val, atomic_index, position);         \
	}

CDSATOMICXOR(8)
CDSATOMICXOR(16)
CDSATOMICXOR(32)
CDSATOMICXOR(64)

// cds atomic compare and exchange
// In order to accomodate the LLVM PASS, the return values are not true or false.

#define _ATOMIC_CMPSWP_WEAK_ _ATOMIC_CMPSWP_
#define _ATOMIC_CMPSWP_(size, addr, expected, desired, atomic_index, position)                            \
	({                                                                                              \
		uint ## size ## _t _desired = desired;                                                            \
		uint ## size ## _t _expected = expected;                                                          \
		uint ## size ## _t _old = model_rmwrcas_action_helper(addr, atomic_index, _expected, sizeof(_expected), position); \
		if (_old == _expected) {                                                                    \
			model_rmw_action_helper(addr, (uint64_t) _desired, atomic_index, position); \
			*((volatile uint ## size ## _t *)addr) = desired;                        \
			thread_id_t tid = thread_current_id();           \
			for(int i=0;i < size / 8;i++) {                       \
				recordWrite(tid, (void *)(((char *)addr)+i));         \
			}                                                       \
			return _expected; }                                     \
		else {                                                                                        \
			model_rmwc_action_helper(addr, atomic_index, position); _expected = _old; return _old; }              \
	})

// atomic_compare_exchange version 1: the CmpOperand (corresponds to expected)
// extracted from LLVM IR is an integer type.
#define CDSATOMICCASV1(size)                                            \
	uint ## size ## _t cds_atomic_compare_exchange ## size ## _v1(void* addr, uint ## size ## _t expected, uint ## size ## _t desired, int atomic_index_succ, int atomic_index_fail, const char *position) { \
		_ATOMIC_CMPSWP_(size, addr, expected, desired, atomic_index_succ, position); \
	}

CDSATOMICCASV1(8)
CDSATOMICCASV1(16)
CDSATOMICCASV1(32)
CDSATOMICCASV1(64)

// atomic_compare_exchange version 2
#define CDSATOMICCASV2(size)                                            \
	bool cds_atomic_compare_exchange ## size ## _v2(void* addr, uint ## size ## _t* expected, uint ## size ## _t desired, int atomic_index_succ, int atomic_index_fail, const char *position) { \
		uint ## size ## _t ret = cds_atomic_compare_exchange ## size ## _v1(addr, *expected, desired, atomic_index_succ, atomic_index_fail, position); \
		if (ret == *expected) {return true;} else {return false;}               \
	}

CDSATOMICCASV2(8)
CDSATOMICCASV2(16)
CDSATOMICCASV2(32)
CDSATOMICCASV2(64)

// cds atomic thread fence

void cds_atomic_thread_fence(int atomic_index, const char * position) {
	model->switch_thread(
		new ModelAction(ATOMIC_FENCE, position, orders[atomic_index], FENCE_LOCATION)
		);
}

/*
 #define _ATOMIC_CMPSWP_( __a__, __e__, __m__, __x__ )                         \
        ({ volatile __typeof__((__a__)->__f__)* __p__ = & ((__a__)->__f__);   \
                __typeof__(__e__) __q__ = (__e__);                            \
                __typeof__(__m__) __v__ = (__m__);                            \
                bool __r__;                                                   \
                __typeof__((__a__)->__f__) __t__=(__typeof__((__a__)->__f__)) model_rmwr_action((void *)__p__, __x__); \
                if (__t__ == * __q__ ) {                                      \
                        model_rmw_action((void *)__p__, __x__, (uint64_t) __v__); __r__ = true; } \
                else {  model_rmwc_action((void *)__p__, __x__); *__q__ = __t__;  __r__ = false;} \
                __r__; })

 #define _ATOMIC_FENCE_( __x__ ) \
        ({ model_fence_action(__x__);})
 */

/*

 #define _ATOMIC_MODIFY_( __a__, __o__, __m__, __x__ )                         \
        ({ volatile __typeof__((__a__)->__f__)* __p__ = & ((__a__)->__f__);   \
        __typeof__((__a__)->__f__) __old__=(__typeof__((__a__)->__f__)) model_rmwr_action((void *)__p__, __x__); \
        __typeof__(__m__) __v__ = (__m__);                                    \
        __typeof__((__a__)->__f__) __copy__= __old__;                         \
        __copy__ __o__ __v__;                                                 \
        model_rmw_action((void *)__p__, __x__, (uint64_t) __copy__);          \
        __old__ = __old__;  Silence clang (-Wunused-value)                    \
         })
 */

void cds_func_entry(const char * funcName) {
#ifdef NEWFUZZER
	createModelIfNotExist();
	thread_id_t tid = thread_current_id();
	uint32_t func_id;

	ModelHistory *history = model->get_history();
	if ( !history->getFuncMap()->contains(funcName) ) {
		// add func id to func map
		func_id = history->get_func_counter();
		history->incr_func_counter();
		history->getFuncMap()->put(funcName, func_id);

		// add func id to reverse func map
		ModelVector<const char *> * func_map_rev = history->getFuncMapRev();
		if ( func_map_rev->size() <= func_id )
			func_map_rev->resize( func_id + 1 );

		func_map_rev->at(func_id) = funcName;
	} else {
		func_id = history->getFuncMap()->get(funcName);
	}

	history->enter_function(func_id, tid);
#endif
}

void cds_func_exit(const char * funcName) {
#ifdef NEWFUZZER
	createModelIfNotExist();
	thread_id_t tid = thread_current_id();
	uint32_t func_id;

	ModelHistory *history = model->get_history();
	func_id = history->getFuncMap()->get(funcName);
/*
 * func_id not found; this could happen in the case where a function calls cds_func_entry
 * when the model has been defined yet, but then an atomic inside the function initializes
 * the model. And then cds_func_exit is called upon the function exiting.
 *
 */
	if (func_id == 0)
		return;

	history->exit_function(func_id, tid);
#endif
}
