/** @file config.h
 * @brief Configuration file.
 */

#ifndef CONFIG_H
#define CONFIG_H

/** Turn on debugging. */
/*		#ifndef CONFIG_DEBUG
 #define CONFIG_DEBUG
 #endif

 #ifndef CONFIG_ASSERT
 #define CONFIG_ASSERT
 #endif
 */

/** Turn on support for dumping cyclegraphs as dot files at each
 *  printed summary.*/
#define SUPPORT_MOD_ORDER_DUMP 0

/** Do we have a 48 bit virtual address (64 bit machine) or 32 bit addresses.
 * Set to 1 for 48-bit, 0 for 32-bit. */
#ifndef BIT48
#ifdef _LP64
#define BIT48 1
#else
#define BIT48 0
#endif
#endif	/* BIT48 */

/** Snapshotting configurables */

/** Size of signal stack */
#define SIGSTACKSIZE 65536

/** Page size configuration */
#define PAGESIZE 4096

#define TLS 1

/** Thread parameters */

/* Size of stack to allocate for a thread. */
#define STACK_SIZE (1024 * 1024)

/** How many shadow tables of memory to preallocate for data race detector. */
#define SHADOWBASETABLES 4

/** Enable debugging assertions (via ASSERT()) */
#define CONFIG_ASSERT

/** Enable mitigations against fork handlers that call into locks...  */
#define FORK_HANDLER_HACK

/** Enable smart fuzzer */
//#define NEWFUZZER

/** Define semantics of volatile memory operations. */
#define memory_order_volatile_load memory_order_acquire
#define memory_order_volatile_store memory_order_release

//#define memory_order_volatile_load memory_order_relaxed
//#define memory_order_volatile_store memory_order_relaxed

//#define COLLECT_STAT
#define REPORT_DATA_RACES

#endif
