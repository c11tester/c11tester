#include "concretepredicate.h"

ConcretePredicate::ConcretePredicate(thread_id_t tid) :
	tid(tid),
	expressions()
{}

void ConcretePredicate::add_expression(token_t token, uint64_t value, bool equality)
{
	expressions.push_back(concrete_pred_expr(token, value, equality));
}
