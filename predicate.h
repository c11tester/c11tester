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
	Predicate(FuncInst * func_inst, bool is_entry = false);
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	PredExprSet * get_pred_expressions() { return &pred_expressions; }
	void add_predicate(token_t token, void * location, bool value);
	void add_child(Predicate * child);
	void add_parent(Predicate * parent);
	void set_backedge(Predicate * back_pred) { backedge = back_pred; }

	ModelVector<Predicate *> * get_children() { return &children; }
	ModelVector<Predicate *> * get_parents() { return &parents; }
	Predicate * get_backedge() { return backedge; }

	bool is_entry_predicate() { return entry_predicate; }
	void set_entry_predicate() { entry_predicate = true; }

	void print_predicate();
	void print_pred_subtree();

	MEMALLOC
private:
	FuncInst * func_inst;
	bool entry_predicate;

	/* may have multiple predicates */
	PredExprSet pred_expressions;
	ModelVector<Predicate *> children;
	ModelVector<Predicate *> parents;

	/* assume almost one back edge exists */
	Predicate * backedge;
};

#endif /* __PREDICATE_H__ */
