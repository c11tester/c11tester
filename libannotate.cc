#include <cdsannotate.h>
#include "common.h"
#include "action.h"
#include "model.h"

/** Pass in an annotation that a trace analysis will use.  The
 *  analysis type is a unique number that specifies which trace
 *  analysis needs the annotation.  The reference is to a data
 *  structure that the trace understands. */

void cdsannotate(uint64_t analysistype, void *annotation) {
	/* seq_cst is just a 'don't care' parameter */
	model->switch_to_master(new ModelAction(ATOMIC_ANNOTATION, std::memory_order_seq_cst, annotation, analysistype));
}
