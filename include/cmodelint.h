/** @file cmodelint.h
 *  @brief C interface to the model checker.
 */

#ifndef CMODELINT_H
#define CMODELINT_H
#include <inttypes.h>
#include "memoryorder.h"

#if __cplusplus
using std::memory_order;
extern "C" {
#endif

uint64_t model_read_action(void * obj, memory_order ord);
void model_write_action(void * obj, memory_order ord, uint64_t val);
void model_init_action(void * obj, uint64_t val);
uint64_t model_rmwr_action(void *obj, memory_order ord);
void model_rmw_action(void *obj, memory_order ord, uint64_t val);
void model_rmwc_action(void *obj, memory_order ord);
void model_fence_action(memory_order ord);

// void model_init_action_helper(void * obj, uint64_t val);
uint64_t model_rmwr_action_helper(void *obj, int atomic_index);
void model_rmw_action_helper(void *obj, int atomic_index, uint64_t val);
void model_rmwc_action_helper(void *obj, int atomic_index);
void model_fence_action_helper(int atomic_index);

/* the following functions are used by llvm pass */

void cds_atomic_init8(void * obj, uint8_t val);
void cds_atomic_init16(void * obj, uint16_t val);
void cds_atomic_init32(void * obj, uint32_t val);
void cds_atomic_init64(void * obj, uint64_t val);

uint8_t  cds_atomic_load8(void * obj, int atomic_index);
uint16_t cds_atomic_load16(void * obj, int atomic_index);
uint32_t cds_atomic_load32(void * obj, int atomic_index);
uint64_t cds_atomic_load64(void * obj, int atomic_index);

void cds_atomic_store8(void * obj, uint8_t val, int atomic_index);
void cds_atomic_store16(void * obj, uint16_t val, int atomic_index);
void cds_atomic_store32(void * obj, uint32_t val, int atomic_index);
void cds_atomic_store64(void * obj, uint64_t val, int atomic_index);


// cds atomic exchange
uint8_t cds_atomic_exchange8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_exchange16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_exchange32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_exchange64(void* addr, uint64_t val, int atomic_index);
// cds atomic fetch add
uint8_t  cds_atomic_fetch_add8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_fetch_add16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_fetch_add32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_fetch_add64(void* addr, uint64_t val, int atomic_index);
// cds atomic fetch sub
uint8_t  cds_atomic_fetch_sub8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_fetch_sub16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_fetch_sub32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_fetch_sub64(void* addr, uint64_t val, int atomic_index);
// cds atomic fetch and
uint8_t cds_atomic_fetch_and8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_fetch_and16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_fetch_and32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_fetch_and64(void* addr, uint64_t val, int atomic_index);
// cds atomic fetch or
uint8_t cds_atomic_fetch_or8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_fetch_or16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_fetch_or32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_fetch_or64(void* addr, uint64_t val, int atomic_index);
// cds atomic fetch xor
uint8_t cds_atomic_fetch_xor8(void* addr, uint8_t val, int atomic_index);
uint16_t cds_atomic_fetch_xor16(void* addr, uint16_t val, int atomic_index);
uint32_t cds_atomic_fetch_xor32(void* addr, uint32_t val, int atomic_index);
uint64_t cds_atomic_fetch_xor64(void* addr, uint64_t val, int atomic_index);

// cds atomic compare and exchange (strong)
uint8_t cds_atomic_compare_exchange8_v1(void* addr, uint8_t expected, 
                  uint8_t desire, int atomic_index_succ, int atomic_index_fail );
uint16_t cds_atomic_compare_exchange16_v1(void* addr, uint16_t expected,
                  uint16_t desire, int atomic_index_succ, int atomic_index_fail );
uint32_t cds_atomic_compare_exchange32_v1(void* addr, uint32_t expected, 
                  uint32_t desire, int atomic_index_succ, int atomic_index_fail );
uint64_t cds_atomic_compare_exchange64_v1(void* addr, uint64_t expected, 
                  uint64_t desire, int atomic_index_succ, int atomic_index_fail );

bool cds_atomic_compare_exchange8_v2(void* addr, uint8_t* expected, 
                uint8_t desired, int atomic_index_succ, int atomic_index_fail );
bool cds_atomic_compare_exchange16_v2(void* addr, uint16_t* expected,
                uint16_t desired, int atomic_index_succ, int atomic_index_fail );
bool cds_atomic_compare_exchange32_v2(void* addr, uint32_t* expected, 
                uint32_t desired, int atomic_index_succ, int atomic_index_fail );
bool cds_atomic_compare_exchange64_v2(void* addr, uint64_t* expected, 
                uint64_t desired, int atomic_index_succ, int atomic_index_fail );

// cds atomic thread fence
void cds_atomic_thread_fence(int atomic_index);

#if __cplusplus
}
#endif

#endif
