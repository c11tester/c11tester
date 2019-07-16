#include <stdio.h>
#include <string>

#include "model.h"
#include "execution.h"
#include "action.h"
#include "history.h"
#include "cmodelint.h"
#include "snapshot-interface.h"
#include "threads-model.h"

memory_order orders[6] = {
	memory_order_relaxed, memory_order_consume, memory_order_acquire,
	memory_order_release, memory_order_acq_rel, memory_order_seq_cst
};

static void ensureModel() {
	if (!model) {
		snapshot_system_init(10000, 1024, 1024, 40000);
		model = new ModelChecker();
		model->startChecker();
	}
}

/** Performs a read action.*/
uint64_t model_read_action(void * obj, memory_order ord) {
	return model->switch_to_master(new ModelAction(ATOMIC_READ, ord, obj));
}

/** Performs a write action.*/
void model_write_action(void * obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, ord, obj, val));
}

/** Performs an init action. */
void model_init_action(void * obj, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_INIT, memory_order_relaxed, obj, val));
}

/**
 * Performs the read part of a RMW action. The next action must either be the
 * write part of the RMW action or an explicit close out of the RMW action w/o
 * a write.
 */
uint64_t model_rmwr_action(void *obj, memory_order ord) {
	return model->switch_to_master(new ModelAction(ATOMIC_RMWR, ord, obj));
}

/**
 * Performs the read part of a RMW CAS action. The next action must
 * either be the write part of the RMW action or an explicit close out
 * of the RMW action w/o a write.
 */
uint64_t model_rmwrcas_action(void *obj, memory_order ord, uint64_t oldval, int size) {
	return model->switch_to_master(new ModelAction(ATOMIC_RMWRCAS, ord, obj, oldval, size));
}


/** Performs the write part of a RMW action. */
void model_rmw_action(void *obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_RMW, ord, obj, val));
}

/** Closes out a RMW action without doing a write. */
void model_rmwc_action(void *obj, memory_order ord) {
	model->switch_to_master(new ModelAction(ATOMIC_RMWC, ord, obj));
}

/** Issues a fence operation. */
void model_fence_action(memory_order ord) {
	model->switch_to_master(new ModelAction(ATOMIC_FENCE, ord, FENCE_LOCATION));
}

/* ---  helper functions --- */
uint64_t model_rmwrcas_action_helper(void *obj, int atomic_index, uint64_t oldval, int size, const char *position) {
	ensureModel();
	return model->switch_to_master(new ModelAction(ATOMIC_RMWRCAS, position, orders[atomic_index], obj, oldval, size));
}

uint64_t model_rmwr_action_helper(void *obj, int atomic_index, const char *position) {
	ensureModel();
	return model->switch_to_master(new ModelAction(ATOMIC_RMWR, position, orders[atomic_index], obj));
}

void model_rmw_action_helper(void *obj, uint64_t val, int atomic_index, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_RMW, position, orders[atomic_index], obj, val));
}

void model_rmwc_action_helper(void *obj, int atomic_index, const char *position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_RMWC, position, orders[atomic_index], obj));
}

// cds atomic inits
void cds_atomic_init8(void * obj, uint8_t val, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_INIT, position, memory_order_relaxed, obj, (uint64_t) val));
}
void cds_atomic_init16(void * obj, uint16_t val, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_INIT, position, memory_order_relaxed, obj, (uint64_t) val));
}
void cds_atomic_init32(void * obj, uint32_t val, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_INIT, position, memory_order_relaxed, obj, (uint64_t) val));
}
void cds_atomic_init64(void * obj, uint64_t val, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_INIT, position, memory_order_relaxed, obj, val));
}


// cds atomic loads
uint8_t cds_atomic_load8(void * obj, int atomic_index, const char * position) {
	ensureModel();
	return (uint8_t) model->switch_to_master(
		new ModelAction(ATOMIC_READ, position, orders[atomic_index], obj));
}
uint16_t cds_atomic_load16(void * obj, int atomic_index, const char * position) {
	ensureModel();
	return (uint16_t) model->switch_to_master(
		new ModelAction(ATOMIC_READ, position, orders[atomic_index], obj));
}
uint32_t cds_atomic_load32(void * obj, int atomic_index, const char * position) {
	ensureModel();
	return (uint32_t) model->switch_to_master(
		new ModelAction(ATOMIC_READ, position, orders[atomic_index], obj)
		);
}
uint64_t cds_atomic_load64(void * obj, int atomic_index, const char * position) {
	ensureModel();
	return model->switch_to_master(
		new ModelAction(ATOMIC_READ, position, orders[atomic_index], obj));
}

// cds atomic stores
void cds_atomic_store8(void * obj, uint8_t val, int atomic_index, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, position, orders[atomic_index], obj, (uint64_t) val));
}
void cds_atomic_store16(void * obj, uint16_t val, int atomic_index, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, position, orders[atomic_index], obj, (uint64_t) val));
}
void cds_atomic_store32(void * obj, uint32_t val, int atomic_index, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, position, orders[atomic_index], obj, (uint64_t) val));
}
void cds_atomic_store64(void * obj, uint64_t val, int atomic_index, const char * position) {
	ensureModel();
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, position, orders[atomic_index], obj, (uint64_t) val));
}

#define _ATOMIC_RMW_(__op__, size, addr, val, atomic_index, position)            \
	({                                                                      \
		uint ## size ## _t _old = model_rmwr_action_helper(addr, atomic_index, position);   \
		uint ## size ## _t _copy = _old;                                          \
		uint ## size ## _t _val = val;                                            \
		_copy __op__ _val;                                                    \
		model_rmw_action_helper(addr, (uint64_t) _copy, atomic_index, position);        \
		return _old;                                                          \
	})

// cds atomic exchange
uint8_t cds_atomic_exchange8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( =, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_exchange16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( =, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_exchange32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( =, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_exchange64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( =, 64, addr, val, atomic_index, position);
}

// cds atomic fetch add
uint8_t cds_atomic_fetch_add8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( +=, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_fetch_add16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( +=, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_fetch_add32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( +=, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_fetch_add64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( +=, 64, addr, val, atomic_index, position);
}

// cds atomic fetch sub
uint8_t cds_atomic_fetch_sub8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( -=, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_fetch_sub16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( -=, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_fetch_sub32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( -=, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_fetch_sub64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( -=, 64, addr, val, atomic_index, position);
}

// cds atomic fetch and
uint8_t cds_atomic_fetch_and8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( &=, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_fetch_and16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( &=, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_fetch_and32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( &=, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_fetch_and64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( &=, 64, addr, val, atomic_index, position);
}

// cds atomic fetch or
uint8_t cds_atomic_fetch_or8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( |=, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_fetch_or16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( |=, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_fetch_or32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( |=, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_fetch_or64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( |=, 64, addr, val, atomic_index, position);
}

// cds atomic fetch xor
uint8_t cds_atomic_fetch_xor8(void* addr, uint8_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( ^=, 8, addr, val, atomic_index, position);
}
uint16_t cds_atomic_fetch_xor16(void* addr, uint16_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( ^=, 16, addr, val, atomic_index, position);
}
uint32_t cds_atomic_fetch_xor32(void* addr, uint32_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( ^=, 32, addr, val, atomic_index, position);
}
uint64_t cds_atomic_fetch_xor64(void* addr, uint64_t val, int atomic_index, const char * position) {
	_ATOMIC_RMW_( ^=, 64, addr, val, atomic_index, position);
}

// cds atomic compare and exchange
// In order to accomodate the LLVM PASS, the return values are not true or false.

#define _ATOMIC_CMPSWP_WEAK_ _ATOMIC_CMPSWP_
#define _ATOMIC_CMPSWP_(size, addr, expected, desired, atomic_index, position)                            \
	({                                                                                              \
		uint ## size ## _t _desired = desired;                                                            \
		uint ## size ## _t _expected = expected;                                                          \
		uint ## size ## _t _old = model_rmwrcas_action_helper(addr, atomic_index, _expected, sizeof(_expected), position); \
		if (_old == _expected) {                                                                    \
			model_rmw_action_helper(addr, (uint64_t) _desired, atomic_index, position); return _expected; }      \
		else {                                                                                        \
			model_rmwc_action_helper(addr, atomic_index, position); _expected = _old; return _old; }              \
	})

// atomic_compare_exchange version 1: the CmpOperand (corresponds to expected)
// extracted from LLVM IR is an integer type.

uint8_t cds_atomic_compare_exchange8_v1(void* addr, uint8_t expected, uint8_t desired,
																				int atomic_index_succ, int atomic_index_fail, const char *position )
{
	_ATOMIC_CMPSWP_(8, addr, expected, desired,
									atomic_index_succ, position);
}
uint16_t cds_atomic_compare_exchange16_v1(void* addr, uint16_t expected, uint16_t desired,
																					int atomic_index_succ, int atomic_index_fail, const char *position)
{
	_ATOMIC_CMPSWP_(16, addr, expected, desired,
									atomic_index_succ, position);
}
uint32_t cds_atomic_compare_exchange32_v1(void* addr, uint32_t expected, uint32_t desired,
																					int atomic_index_succ, int atomic_index_fail, const char *position)
{
	_ATOMIC_CMPSWP_(32, addr, expected, desired,
									atomic_index_succ, position);
}
uint64_t cds_atomic_compare_exchange64_v1(void* addr, uint64_t expected, uint64_t desired,
																					int atomic_index_succ, int atomic_index_fail, const char *position)
{
	_ATOMIC_CMPSWP_(64, addr, expected, desired,
									atomic_index_succ, position);
}

// atomic_compare_exchange version 2
bool cds_atomic_compare_exchange8_v2(void* addr, uint8_t* expected, uint8_t desired,
																		 int atomic_index_succ, int atomic_index_fail, const char *position )
{
	uint8_t ret = cds_atomic_compare_exchange8_v1(addr, *expected,
																								desired, atomic_index_succ, atomic_index_fail, position);
	if (ret == *expected) return true;
	else return false;
}
bool cds_atomic_compare_exchange16_v2(void* addr, uint16_t* expected, uint16_t desired,
																			int atomic_index_succ, int atomic_index_fail, const char *position)
{
	uint16_t ret = cds_atomic_compare_exchange16_v1(addr, *expected,
																									desired, atomic_index_succ, atomic_index_fail, position);
	if (ret == *expected) return true;
	else return false;
}
bool cds_atomic_compare_exchange32_v2(void* addr, uint32_t* expected, uint32_t desired,
																			int atomic_index_succ, int atomic_index_fail, const char *position)
{
	uint32_t ret = cds_atomic_compare_exchange32_v1(addr, *expected,
																									desired, atomic_index_succ, atomic_index_fail, position);
	if (ret == *expected) return true;
	else return false;
}
bool cds_atomic_compare_exchange64_v2(void* addr, uint64_t* expected, uint64_t desired,
																			int atomic_index_succ, int atomic_index_fail, const char *position)
{
	uint64_t ret = cds_atomic_compare_exchange64_v1(addr, *expected,
																									desired, atomic_index_succ, atomic_index_fail, position);
	if (ret == *expected) return true;
	else return false;
}


// cds atomic thread fence

void cds_atomic_thread_fence(int atomic_index, const char * position) {
	model->switch_to_master(
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
	if (!model) return;

	Thread * th = thread_current();
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

	history->enter_function(func_id, th->get_id());
}

void cds_func_exit(const char * funcName) {
	if (!model) return;

	Thread * th = thread_current();
	uint32_t func_id;

	ModelHistory *history = model->get_history();
	func_id = history->getFuncMap()->get(funcName);

	/* func_id not found; this could happen in the case where a function calls cds_func_entry
	 * when the model has been defined yet, but then an atomic inside the function initializes
	 * the model. And then cds_func_exit is called upon the function exiting.
	 */
	if (func_id == 0)
		return;

	history->exit_function(func_id, th->get_id());
}
