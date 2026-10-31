// State shared by the type checker files.
//
// Expressions are checked bidirectionally: check_expr receives the type the
// context expects (TID_ERROR when there is none) as a hint, synthesizes the
// type and stores it in the node; `coerce` then reports a mismatch.
// Divergence is a type: `return`, `break` and infinite loops have type `!`.

#ifndef ASTRA_CHECK_INTERNAL_H
#define ASTRA_CHECK_INTERNAL_H

#include "sema/sema.h"

typedef struct {
    Sema *sema;
    TypeTable *types;
    FnDecl *fn;              // the function being checked
    TypeId ret;              // its return type
    TypeId self_type;        // the type declaring the method, or TID_ERROR
    TypeId *local_types;     // indexed by LocalId
    Vec(bool) loops;         // per open loop: whether a `break` leaves it
} Checker;

#define NO_TYPE type_id(TID_ERROR)   // "no expected type" for check_expr

void check_error(Checker *c, const char *code, Span span, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

// Reports E0901 for a construct the stage 0 checker does not implement yet.
TypeId unsupported(Checker *c, Span span, const char *what);

// The spelling of a type for messages; valid until the next call.
const char *type_name(Checker *c, TypeId type);

// check_types.c
TypeId resolve_type(Checker *c, const TypeExpr *type);
TypeId fn_signature(Checker *c, FnDecl *fn);   // the TY_FN type, cached in the Def
bool types_compatible(TypeId found, TypeId expected);

// check_expr.c
TypeId check_expr(Checker *c, Expr *e, TypeId expected);
TypeId coerce(Checker *c, Expr *e, TypeId expected);
TypeId check_const(Checker *c, Def *def);   // the type of a constant or global
TypeId check_args(Checker *c, Expr *call, const Ty *sig);

// const_eval.c: the value of a constant's initializer, or NULL after an error
struct ConstValue *const_eval(Checker *c, const Expr *e);

// check_method.c: methods, fields and indexing
TypeId check_method_call(Checker *c, Expr *call);
TypeId check_field(Checker *c, Expr *e);
TypeId check_index(Checker *c, Expr *e);
// Reports E0315 unless `e` is a place that may be modified.
void require_mutable(Checker *c, Expr *e);
// The variable at the root of a place: `p` in `p.pos.x`, or NULL.
Expr *place_root(Expr *e);

// check_user_method.c
bool type_name_of(Checker *c, const Expr *e, DefId *out);
void check_mut_args(Checker *c, const FnDecl *fn, Expr *call);
TypeId check_user_method(Checker *c, Expr *call, DefId owner, bool via_type);

// structs.c
StructInfo *struct_fields(Checker *c, DefId id);   // NULL for generic structs
TypeId check_struct_field(Checker *c, Expr *e, TypeId base);
TypeId check_tuple_field(Checker *c, Expr *e);
TypeId check_constructor(Checker *c, Expr *call, DefId id);
TypeId check_struct_literal(Checker *c, Expr *e);
void check_not_recursive(Checker *c, TypeId type, Span span);

// enums.c
EnumInfo *enum_variants(Checker *c, DefId id);   // NULL for generic enums
EnumInfo *enum_of(Checker *c, TypeId type, DefId *id);
int64_t variant_index(const EnumInfo *info, Symbol name);
TypeId check_variant_shorthand(Checker *c, Expr *e, TypeId expected);
bool check_qualified_variant(Checker *c, Expr *e, DefId id, Symbol name, TypeId *out);

// check_match.c
TypeId check_match(Checker *c, Expr *e, TypeId expected, bool as_value);

// check_pattern.c
// Checks `p` against a value of `type` and gives its bindings their types.
void check_pattern(Checker *c, Pattern *p, TypeId type);
// Whether `p` matches every value of its type (`let` needs such a pattern).
bool pattern_irrefutable(Checker *c, const Pattern *p);

// check_stmt.c
TypeId check_block(Checker *c, Block *block, TypeId expected, bool as_value);
TypeId check_if(Checker *c, Expr *e, TypeId expected, bool as_value);

#endif
