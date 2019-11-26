/**
 * @file predicatetypes.h
 * @brief Define common predicate expression types
 */

#ifndef __PREDICATE_TYPES_H__
#define __PREDICATE_TYPES_H__

typedef enum predicate_token {
	NOPREDICATE, EQUALITY, NULLITY
} token_t;

typedef enum predicate_sleep_result {
	SLEEP_FAIL_TYPE1, SLEEP_FAIL_TYPE2, SLEEP_FAIL_TYPE3,
	SLEEP_SUCCESS
} sleep_result_t;

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

/* Used by FuncNode to generate Predicates */
struct half_pred_expr {
	half_pred_expr(token_t token, FuncInst * inst) :
		token(token),
		func_inst(inst)
	{}

	token_t token;
	FuncInst * func_inst;

	SNAPSHOTALLOC
};

struct concrete_pred_expr {
	concrete_pred_expr(token_t token, uint64_t value, bool equality) :
		token(token),
		value(value),
		equality(equality)
	{}

	token_t token;
	uint64_t value;
	bool equality;

	SNAPSHOTALLOC
};

#endif	/* __PREDICATE_TYPES_H__ */

