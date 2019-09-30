#ifndef __CONCRETE_PREDICATE_H__
#define __CONCRETE_PREDICATE_H__

#include <inttypes.h>
#include "classlist.h"
#include "predicatetypes.h"

class ConcretePredicate {
public:
	ConcretePredicate(void * loc);
	~ConcretePredicate();

	void add_expression(token_t token, uint64_t value, bool equality);
	SnapVector<struct concrete_pred_expr> * getExpressions() { return &expressions; }
	void * get_location() { return location; }

	SNAPSHOTALLOC
private:
	void * location;
	SnapVector<struct concrete_pred_expr> expressions;
};

#endif /* __CONCRETE_PREDICATE_H */
