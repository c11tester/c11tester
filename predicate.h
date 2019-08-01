#ifndef __PREDICTAE_H__
#define __PREDICATE_H__

#include "funcinst.h"
#include "hashset.h"

unsigned int pred_expr_hash (struct pred_expr *);
bool pred_expr_equal(struct pred_expr *, struct pred_expr *);
typedef HashSet<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredicateSet;

typedef enum predicate_token {
	EQUALITY, NULLITY
} token_t;

/* If token is EQUALITY, then the predicate asserts whether
 * this load should read the same value as the last value
 * read at memory location specified in predicate_expr.
 */
struct pred_expr {
	token_t token;
	void * location;
	bool value;
};


class Predicate {
public:
	Predicate(FuncInst * func_inst);
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	PredicateSet * get_predicates() { return &predicates; }
	void add_predicate(token_t token, void * location, bool value);

	MEMALLOC
private:
	FuncInst * func_inst;
	/* may have multiple precicates */
	PredicateSet predicates;
	ModelVector<Predicate *> children;
};

#endif /* __PREDICATE_H__ */
