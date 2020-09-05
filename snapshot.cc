#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "hashtable.h"
#include "snapshot.h"
#include "mymemory.h"
#include "common.h"
#include "context.h"
#include "model.h"


#define SHARED_MEMORY_DEFAULT  (200 * ((size_t)1 << 20))	// 100mb for the shared memory
#define STACK_SIZE_DEFAULT      (((size_t)1 << 20) * 20)	// 20 mb out of the above 100 mb for my stack

struct fork_snapshotter {
	/** @brief Pointer to the shared (non-snapshot) memory heap base
	 * (NOTE: this has size SHARED_MEMORY_DEFAULT - sizeof(*fork_snap)) */
	void *mSharedMemoryBase;

	/** @brief Pointer to the shared (non-snapshot) stack region */
	void *mStackBase;

	/** @brief Size of the shared stack */
	size_t mStackSize;

	/**
	 * @brief Stores the ID that we are attempting to roll back to
	 *
	 * Used in inter-process communication so that each process can
	 * determine whether or not to take over execution (w/ matching ID) or
	 * exit (we're rolling back even further). Dubiously marked 'volatile'
	 * to prevent compiler optimizations from messing with the
	 * inter-process behavior.
	 */
	volatile snapshot_id mIDToRollback;



	/** @brief Inter-process tracking of the next snapshot ID */
	snapshot_id currSnapShotID;
};

static struct fork_snapshotter *fork_snap = NULL;
ucontext_t shared_ctxt;

/** @statics
 *   These variables are necessary because the stack is shared region and
 *   there exists a race between all processes executing the same function.
 *   To avoid the problem above, we require variables allocated in 'safe' regions.
 *   The bug was actually observed with the forkID, these variables below are
 *   used to indicate the various contexts to which to switch to.
 *
 *   @private_ctxt: the context which is internal to the current process. Used
 *   for running the internal snapshot/rollback loop.
 *   @exit_ctxt: a special context used just for exiting from a process (so we
 *   can use swapcontext() instead of setcontext() + hacks)
 *   @snapshotid: it is a running counter for the various forked processes
 *   snapshotid. it is incremented and set in a persistently shared record
 */
static ucontext_t private_ctxt;
static snapshot_id snapshotid = 0;

/**
 * @brief Create a new context, with a given stack and entry function
 * @param ctxt The context structure to fill
 * @param stack The stack to run the new context in
 * @param stacksize The size of the stack
 * @param func The entry point function for the context
 */
static void create_context(ucontext_t *ctxt, void *stack, size_t stacksize,
													 void (*func)(void))
{
	getcontext(ctxt);
	ctxt->uc_stack.ss_sp = stack;
	ctxt->uc_stack.ss_size = stacksize;
	ctxt->uc_link = NULL;
	makecontext(ctxt, func, 0);
}

/** @brief An empty function, used for an "empty" context which just exits a
 *  process */
static void fork_exit()
{
	_Exit(EXIT_SUCCESS);
}

static void createSharedMemory()
{
	//step 1. create shared memory.
	void *memMapBase = mmap(0, SHARED_MEMORY_DEFAULT + STACK_SIZE_DEFAULT, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	if (memMapBase == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	//Setup snapshot record at top of free region
	fork_snap = (struct fork_snapshotter *)memMapBase;
	fork_snap->mSharedMemoryBase = (void *)((uintptr_t)memMapBase + sizeof(*fork_snap));
	fork_snap->mStackBase = (void *)((uintptr_t)memMapBase + SHARED_MEMORY_DEFAULT);
	fork_snap->mStackSize = STACK_SIZE_DEFAULT;
	fork_snap->mIDToRollback = -1;
	fork_snap->currSnapShotID = 0;
	sStaticSpace = create_shared_mspace();
}

/**
 * Create a new mspace pointer for the non-snapshotting (i.e., inter-process
 * shared) memory region. Only for fork-based snapshotting.
 *
 * @return The shared memory mspace
 */
mspace create_shared_mspace()
{
	if (!fork_snap)
		createSharedMemory();
	return create_mspace_with_base((void *)(fork_snap->mSharedMemoryBase), SHARED_MEMORY_DEFAULT - sizeof(*fork_snap), 1);
}

static void fork_snapshot_init(unsigned int numheappages)
{
	if (!fork_snap)
		createSharedMemory();

	model_snapshot_space = create_mspace(numheappages * PAGESIZE, 1);
}

volatile int modellock = 0;

static void fork_loop() {
	/* switch back here when takesnapshot is called */
	snapshotid = fork_snap->currSnapShotID;
	if (model->params.nofork) {
		setcontext(&shared_ctxt);
		_Exit(EXIT_SUCCESS);
	}

	while (true) {
		pid_t forkedID;
		fork_snap->currSnapShotID = snapshotid + 1;

		modellock = 1;
		forkedID = fork();
		modellock = 0;

		if (0 == forkedID) {
			setcontext(&shared_ctxt);
		} else {
			DEBUG("parent PID: %d, child PID: %d, snapshot ID: %d\n",
						getpid(), forkedID, snapshotid);

			while (waitpid(forkedID, NULL, 0) < 0) {
				/* waitpid() may be interrupted */
				if (errno != EINTR) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
			}

			if (fork_snap->mIDToRollback != snapshotid)
				_Exit(EXIT_SUCCESS);
		}
	}
}

static void fork_startExecution() {
	/* switch to a new entryPoint context, on a new stack */
	create_context(&private_ctxt, snapshot_calloc(STACK_SIZE_DEFAULT, 1), STACK_SIZE_DEFAULT, fork_loop);
}

static snapshot_id fork_take_snapshot() {
	model_swapcontext(&shared_ctxt, &private_ctxt);
	DEBUG("TAKESNAPSHOT RETURN\n");
	fork_snap->mIDToRollback = -1;
	return snapshotid;
}

static void fork_roll_back(snapshot_id theID)
{
	DEBUG("Rollback\n");
	fork_snap->mIDToRollback = theID;
	fork_exit();
}

/**
 * @brief Initializes the snapshot system
 * @param entryPoint the function that should run the program.
 */
void snapshot_system_init(unsigned int numheappages)
{
	fork_snapshot_init(numheappages);
}

void startExecution() {
	fork_startExecution();
}

/** Takes a snapshot of memory.
 * @return The snapshot identifier.
 */
snapshot_id take_snapshot()
{
	return fork_take_snapshot();
}

/** Rolls the memory state back to the given snapshot identifier.
 *  @param theID is the snapshot identifier to rollback to.
 */
void snapshot_roll_back(snapshot_id theID)
{
	fork_roll_back(theID);
}
