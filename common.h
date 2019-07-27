/** @file common.h
 *  @brief General purpose macros.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "printf.h"

extern int model_out;

#define model_print(fmt, ...) do { \
		char mprintbuf[2048];                                                \
		int printbuflen=snprintf_(mprintbuf, 2048, fmt, ## __VA_ARGS__);     \
		int lenleft = printbuflen < 2048 ? printbuflen : 2048;                   \
		int totalwritten = 0; \
		while(lenleft) {                                                    \
			int byteswritten=write(model_out, &mprintbuf[totalwritten], lenleft); \
			lenleft-=byteswritten;                                            \
			totalwritten+=byteswritten;                                       \
		}                                                                   \
} while (0)

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { model_print("*** %15s:%-4d %25s() *** " fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__); } while (0)
#define DBG() DEBUG("\n")
#define DBG_ENABLED() (1)
#else
#define DEBUG(fmt, ...)
#define DBG()
#define DBG_ENABLED() (0)
#endif

void assert_hook(void);

#ifdef CONFIG_ASSERT
#define ASSERT(expr) \
	do { \
		if (!(expr)) { \
			fprintf(stderr, "Error: assertion failed in %s at line %d\n", __FILE__, __LINE__); \
			/* print_trace(); // Trace printing may cause dynamic memory allocation */ \
			assert_hook();                           \
			_Exit(EXIT_FAILURE); \
		} \
	} while (0)
#else
#define ASSERT(expr) \
	do { } while (0)
#endif	/* CONFIG_ASSERT */

#define error_msg(...) fprintf(stderr, "Error: " __VA_ARGS__)

void print_trace(void);
#endif	/* __COMMON_H__ */
