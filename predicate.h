#ifndef __PREDICTAE_H__
#define __PREDICATE_H__

#include "funcinst.h"
#include "hashset.h"

unsigned int pred_expr_hash (struct pred_expr *);
bool pred_expr_equal(struct pred_expr *, struct pred_expr *);
typedef HashSet<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSet;
typedef HSIterator<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSetIter;

typedef enum predicate_token {
	NOPREDICATE, EQUALITY, NULLITY
} token_t;

/* If token is EQUALITY, then the predicate asserts whether
 * this load should read the same value as the last value
 * read at memory location specified in predicate_expr.
 */
struct pred_expr {
	pred_expr(token_t token, FuncInst * inst, bool value) :
		token(token),
		func_inst(inst),
		value(value)
	{}

	token_t token;
	FuncInst * func_inst;
	bool value;

	MEMALLOC
};


class Predicate {
public:
	Predicate(FuncInst * func_inst, bool is_entry = false);
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	PredExprSet * get_pred_expressions() { return &pred_expressions; }

	void add_predicate_expr(token_t token, FuncInst * func_inst, bool value);
	void add_child(Predicate * child);
	void set_parent(Predicate * parent_pred) { parent = parent_pred; }
	void add_backedge(Predicate * back_pred) { backedges.add(back_pred); }
	void copy_predicate_expr(Predicate * other);

	ModelVector<Predicate *> * get_children() { return &children; }
	Predicate * get_parent() { return parent; }
	PredSet * get_backedges() { return &backedges; }

	bool is_entry_predicate() { return entry_predicate; }
	void set_entry_predicate() { entry_predicate = true; }

	void print_predicate();
	void print_pred_subtree();

	MEMALLOC
private:
	FuncInst * func_inst;
	bool entry_predicate;

	/* may have multiple predicate expressions */
	PredExprSet pred_expressions;
	ModelVector<Predicate *> children;

	/* only a single parent may exist */
	Predicate * parent;

	/* may have multiple back edges, e.g. nested loops */
	PredSet backedges;
};

#endif /* __PREDICATE_H__ */
