#ifndef _FENCE_COMMON_
#define _FENCE_COMMON_

#include "model.h"
#include "action.h"

#define DEFAULT_REPETITIVE_READ_BOUND 20

#define FENCE_OUTPUT

#ifdef FENCE_OUTPUT

#define FENCE_PRINT model_print

#define ACT_PRINT(x) (x)->print()

#define CV_PRINT(x) (x)->print()

#define WILDCARD_ACT_PRINT(x)\
	FENCE_PRINT("Wildcard: %d\n", get_wildcard_id_zero((x)->get_original_mo()));\
	ACT_PRINT(x);

#else

#define FENCE_PRINT

#define ACT_PRINT(x)

#define CV_PRINT(x)

#define WILDCARD_ACT_PRINT(x)

#endif

#endif
