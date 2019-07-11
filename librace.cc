#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "librace.h"
#include "common.h"
#include "datarace.h"
#include "model.h"
#include "threads-model.h"
#include "snapshot-interface.h"

void store_8(void *addr, uint8_t val)
{
	DEBUG("addr = %p, val = %" PRIu8 "\n", addr, val);
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	(*(uint8_t *)addr) = val;
}

void store_16(void *addr, uint16_t val)
{
	DEBUG("addr = %p, val = %" PRIu16 "\n", addr, val);
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
	(*(uint16_t *)addr) = val;
}

void store_32(void *addr, uint32_t val)
{
	DEBUG("addr = %p, val = %" PRIu32 "\n", addr, val);
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 2));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 3));
	(*(uint32_t *)addr) = val;
}

void store_64(void *addr, uint64_t val)
{
	DEBUG("addr = %p, val = %" PRIu64 "\n", addr, val);
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 2));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 3));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 4));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 5));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 6));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 7));
	(*(uint64_t *)addr) = val;
}

uint8_t load_8(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	return *((uint8_t *)addr);
}

uint16_t load_16(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
	return *((uint16_t *)addr);
}

uint32_t load_32(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 2));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 3));
	return *((uint32_t *)addr);
}

uint64_t load_64(const void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 2));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 3));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 4));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 5));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 6));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 7));
	return *((uint64_t *)addr);
}

// helper functions used by CdsPass
// The CdsPass implementation does not replace normal load/stores with cds load/stores,
// but inserts cds load/stores to check dataraces. Thus, the cds load/stores do not
// return anything.

void cds_store8(void *addr)
{
	//DEBUG("addr = %p, val = %" PRIu8 "\n", addr, val);
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
}

void cds_store16(void *addr)
{
	//DEBUG("addr = %p, val = %" PRIu16 "\n", addr, val);
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
}

void cds_store32(void *addr)
{
	//DEBUG("addr = %p, val = %" PRIu32 "\n", addr, val);
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 2));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 3));
}

void cds_store64(void *addr)
{
	//DEBUG("addr = %p, val = %" PRIu64 "\n", addr, val);
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckWrite(tid, addr);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 1));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 2));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 3));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 4));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 5));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 6));
	raceCheckWrite(tid, (void *)(((uintptr_t)addr) + 7));
}

void cds_load8(const void *addr) {
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
}

void cds_load16(const void *addr) {
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
}

void cds_load32(const void *addr) {
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 2));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 3));
}

void cds_load64(const void *addr) {
	if (!model)
		return;
	thread_id_t tid = thread_current()->get_id();
	raceCheckRead(tid, addr);
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 1));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 2));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 3));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 4));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 5));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 6));
	raceCheckRead(tid, (const void *)(((uintptr_t)addr) + 7));
}
