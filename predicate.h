#ifndef __PREDICTAE_H__
#define __PREDICATE_H__

#include "funcinst.h"
#include "hashset.h"

unsigned int pred_expr_hash (struct pred_expr *);
bool pred_expr_equal(struct pred_expr *, struct pred_expr *);
typedef HashSet<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSet;
typedef HSIterator<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSetIter;

typedef enum predicate_token {
	EQUALITY, NULLITY
} token_t;

/* If token is EQUALITY, then the predicate asserts whether
 * this load should read the same value as the last value
 * read at memory location specified in predicate_expr.
 */
struct pred_expr {
	pred_expr(token_t token, void * location, bool value) :
		token(token),
		location(location),
		value(value)
	{}

	token_t token;
	void * location;
	bool value;

	MEMALLOC
};


class Predicate {
public:
	Predicate(FuncInst * func_inst);
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	PredExprSet * get_pred_expressions() { return &pred_expressions; }
	void add_predicate(token_t token, void * location, bool value);
	void add_child(Predicate * child);
	ModelVector<Predicate *> * get_children() { return &children; }

	void print_predicate();
	void print_pred_subtree();

	MEMALLOC
private:
	FuncInst * func_inst;
	/* may have multiple precicates */
	PredExprSet pred_expressions;
	ModelVector<Predicate *> children;
};

#endif /* __PREDICATE_H__ */
