#ifndef __CONCRETE_PREDICATE_H__
#define __CONCRETE_PREDICATE_H__

#include <inttypes.h>
#include "modeltypes.h"
#include "classlist.h"
#include "predicatetypes.h"

class ConcretePredicate {
public:
	ConcretePredicate(thread_id_t tid);
	~ConcretePredicate() {}

	void add_expression(token_t token, uint64_t value, bool equality);
	SnapVector<struct concrete_pred_expr> * getExpressions() { return &expressions; }
	void set_location(void * loc) { location = loc; }
	void * get_location() { return location; }
	thread_id_t get_tid() { return tid; }

	SNAPSHOTALLOC
private:
	thread_id_t tid;
	void * location;
	SnapVector<struct concrete_pred_expr> expressions;
};

#endif	/* __CONCRETE_PREDICATE_H */
