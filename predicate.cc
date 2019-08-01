#include "predicate.h"

Predicate::Predicate(FuncInst * func_inst) :
	func_inst(func_inst)
{}

unsigned int pred_expr_hash(struct pred_expr * expr) {
        return (unsigned int)((uintptr_t)expr);
}

bool pred_expr_equal(struct pred_expr * p1, struct pred_expr * p2)
{
	if (p1->token != p2->token)
		return false;
	if (p1->token == EQUALITY && p1->location != p2->location)
		return false;
	if (p1->value != p2->value)
		return false;
	return true;
}

void Predicate::add_predicate(token_t token, void * location, bool value)
{
	struct pred_expr predicate = {token, location, value};
	predicates.add(&predicate);
}
