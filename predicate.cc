#include "predicate.h"

inline bool operator==(const predicate_expr& expr_A, const predicate_expr& expr_B)
{
	if (expr_A.token != expr_B.token)
		return false;

	if (expr_A.token == EQUALITY && expr_A.location != expr_B.location)
		return false;

	if (expr_A.value != expr_B.value)
		return false;

	return true;
}

void Predicate::add_predicate(predicate_expr predicate)
{
	ModelList<predicate_expr>::iterator it;
	for (it = predicates.begin(); it != predicates.end(); it++) {
		if (predicate == *it)
			return;
	}

	predicates.push_back(predicate);
}
