
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <new>

#include "mymemory.h"
#include "snapshot.h"
#include "common.h"
#include "threads-model.h"
#include "model.h"
#include "datarace.h"

#define REQUESTS_BEFORE_ALLOC 1024

size_t allocatedReqs[REQUESTS_BEFORE_ALLOC] = { 0 };
int nextRequest = 0;
int howManyFreed = 0;
static mspace sStaticSpace = NULL;

/** Non-snapshotting calloc for our use. */
void *model_calloc(size_t count, size_t size)
{
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_calloc(sStaticSpace, count, size);
}

/** Non-snapshotting malloc for our use. */
void *model_malloc(size_t size)
{
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_malloc(sStaticSpace, size);
}

/** Non-snapshotting malloc for our use. */
void *model_realloc(void *ptr, size_t size)
{
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_realloc(sStaticSpace, ptr, size);
}

/** @brief Snapshotting malloc, for use by model-checker (not user progs) */
void * snapshot_malloc(size_t size)
{
	void *tmp = mspace_malloc(model_snapshot_space, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting calloc, for use by model-checker (not user progs) */
void * snapshot_calloc(size_t count, size_t size)
{
	void *tmp = mspace_calloc(model_snapshot_space, count, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting realloc, for use by model-checker (not user progs) */
void *snapshot_realloc(void *ptr, size_t size)
{
	void *tmp = mspace_realloc(model_snapshot_space, ptr, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting free, for use by model-checker (not user progs) */
void snapshot_free(void *ptr)
{
	mspace_free(model_snapshot_space, ptr);
}

/** Non-snapshotting free for our use. */
void model_free(void *ptr)
{
	mspace_free(sStaticSpace, ptr);
}

/** Bootstrap allocation. Problem is that the dynamic linker calls require
 *  calloc to work and calloc requires the dynamic linker to work. */

#define BOOTSTRAPBYTES 131072
char bootstrapmemory[BOOTSTRAPBYTES];
size_t offset = 0;

void * HandleEarlyAllocationRequest(size_t sz)
{
	/* Align to 8 byte boundary */
	sz = (sz + 7) & ~7;

	if (sz > (BOOTSTRAPBYTES-offset)) {
		model_print("OUT OF BOOTSTRAP MEMORY.  Increase the size of BOOTSTRAPBYTES in mymemory.cc\n");
		exit(EXIT_FAILURE);
	}

	void *pointer = (void *)&bootstrapmemory[offset];
	offset += sz;
	return pointer;
}

/** @brief Global mspace reference for the model-checker's snapshotting heap */
mspace model_snapshot_space = NULL;

/** @brief Snapshotting allocation function for use by the Thread class only */
void * Thread_malloc(size_t size)
{
	return snapshot_malloc(size);
}

/** @brief Snapshotting free function for use by the Thread class only */
void Thread_free(void *ptr)
{
	snapshot_free(ptr);
}
