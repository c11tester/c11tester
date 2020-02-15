#ifndef __PREDICATE_H__
#define __PREDICATE_H__

#include "hashset.h"
#include "predicatetypes.h"
#include "classlist.h"

#define MAX_DEPTH 0x7fffffff

unsigned int pred_expr_hash (struct pred_expr *);
bool pred_expr_equal(struct pred_expr *, struct pred_expr *);
typedef HashSet<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSet;
typedef HSIterator<struct pred_expr *, uintptr_t, 0, model_malloc, model_calloc, model_free, pred_expr_hash, pred_expr_equal> PredExprSetIter;

class Predicate {
public:
	Predicate(FuncInst * func_inst, bool is_entry = false, bool is_exit = false);
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	PredExprSet * get_pred_expressions() { return &pred_expressions; }

	void add_predicate_expr(token_t token, FuncInst * func_inst, bool value);
	void add_child(Predicate * child);
	void set_parent(Predicate * parent_pred);
	void set_exit(Predicate * exit_pred);
	void add_backedge(Predicate * back_pred) { backedges.add(back_pred); }
	void set_weight(double weight_) { weight = weight_; }
	void copy_predicate_expr(Predicate * other);

	Predicate * follow_write_child(FuncInst * inst);
	ModelVector<Predicate *> * get_children() { return &children; }
	Predicate * get_parent() { return parent; }
	Predicate * get_exit() { return exit; }
	PredSet * get_backedges() { return &backedges; }
	double get_weight() { return weight; }

	bool is_entry_predicate() { return entry_predicate; }
	void set_entry_predicate() { entry_predicate = true; }

	/* Whether func_inst does write or not */
	bool is_write() { return does_write; }
	void set_write(bool is_write) { does_write = is_write; }

	ConcretePredicate * evaluate(thread_id_t tid);

	uint32_t get_expl_count() { return exploration_count; }
	uint32_t get_fail_count() { return failure_count; }
	uint32_t get_store_visible_count() { return store_visible_count; }
	uint32_t get_total_checking_count() { return total_checking_count; }

	void incr_expl_count() { exploration_count++; }
	void incr_fail_count() { failure_count++; }
	void incr_store_visible_count() { store_visible_count++; }
	void incr_total_checking_count() { total_checking_count++; }

	uint32_t get_depth() { return depth; }
	void set_depth(uint32_t depth_) { depth = depth_; }

	void print_predicate();
	void print_pred_subtree();

	MEMALLOC
private:
	FuncInst * func_inst;
	bool entry_predicate;
	bool exit_predicate;
	bool does_write;

	/* Height of this predicate node in the predicate tree */
	uint32_t depth;
	double weight;

	uint32_t exploration_count;
	uint32_t failure_count;
	uint32_t store_visible_count;
	uint32_t total_checking_count;	/* The number of times the store visibility is checked */

	/* May have multiple predicate expressions */
	PredExprSet pred_expressions;
	ModelVector<Predicate *> children;

	/* Only a single parent may exist */
	Predicate * parent;
	Predicate * exit;

	/* May have multiple back edges, e.g. nested loops */
	PredSet backedges;
};

#endif	/* __PREDICATE_H__ */
