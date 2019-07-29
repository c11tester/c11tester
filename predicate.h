#include "funcinst.h"

typedef enum predicate_token {
	EQUALITY, NULLITY
} token_t;

/* If token is EQUALITY, then the predicate asserts whether
 * this load should read the same value as the last value 
 * read at memory location specified in predicate_expr.
 */
struct predicate_expr {
	token_t token;
	void * location;
	bool value;
};

class Predicate {
public:
	Predicate();
	~Predicate();

	FuncInst * get_func_inst() { return func_inst; }
	ModelList<predicate_expr> * get_predicates() { return &predicates; }
	void add_predicate(predicate_expr predicate);

	MEMALLOC
private:
	FuncInst * func_inst;
	/* may have multiple precicates */
	ModelList<predicate_expr> predicates;
};
