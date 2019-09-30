#include "concretepredicate.h"

ConcretePredicate::ConcretePredicate(void * loc) :
	location(loc),
	expressions()
{}

ConcretePredicate::~ConcretePredicate()
{
	expressions.clear();
}

void ConcretePredicate::add_expression(token_t token, uint64_t value, bool equality)
{
	expressions.push_back(concrete_pred_expr(token, value, equality));
}
