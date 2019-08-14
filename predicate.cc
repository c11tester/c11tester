#include "predicate.h"

Predicate::Predicate(FuncInst * func_inst, bool is_entry) :
	func_inst(func_inst),
	entry_predicate(is_entry),
	pred_expressions(),
	children(),
	parent(NULL),
	backedge(NULL)
{}

unsigned int pred_expr_hash(struct pred_expr * expr)
{
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
	struct pred_expr *ptr = new pred_expr(token, location, value);
	pred_expressions.add(ptr);
}

void Predicate::add_child(Predicate * child)
{
	/* check duplication? */
	children.push_back(child);
}

void Predicate::print_predicate()
{
	model_print("\"%p\" [shape=box, label=\"\n", this);
	if (entry_predicate) {
		model_print("entry node\"];\n");
		return;
	}

	func_inst->print();

	PredExprSetIter * it = pred_expressions.iterator();

	if (pred_expressions.getSize() == 0)
		model_print("no predicate\n");

	while (it->hasNext()) {
		struct pred_expr * expr = it->next();
		model_print("predicate: token: %d, location: %p, value: %d\n", expr->token, expr->location, expr->value);
	}
	model_print("\"];\n");
}

void Predicate::print_pred_subtree()
{
	print_predicate();
	for (uint i = 0; i < children.size(); i++) {
		Predicate * child = children[i];
		child->print_pred_subtree();
		model_print("\"%p\" -> \"%p\"\n", this, child);
	}

	if (backedge != NULL)
		model_print("\"%p\" -> \"%p\"[style=dashed, color=grey]\n", this, backedge);
}
