// State shared by the lowering files (typed AST → AIR).
//
// AIR local 0 is the return value and AST local `i` is AIR local `i + 1`, so
// `self` and the parameters (the first AST locals) are AIR locals
// 1..param_count. Temporaries follow.

#ifndef ASTRA_LOWER_INTERNAL_H
#define ASTRA_LOWER_INTERNAL_H

#include "air/lower.h"

typedef struct {
    BlockId break_to, continue_to;
} Loop;

typedef struct {
    LocalId local;
    FieldPath path;
} Place;

// A function to compile: a function that is not generic (targs `()`), or
// an instance of a generic one.
typedef struct {
    FnDecl *fn;
    TypeId targs;
} Instance;

typedef struct {
    Sema *sema;
    TypeTable *types;
    AirBody *body;
    BlockId block;          // where the next statement goes
    Vec(Loop) loops;
    Vec(Instance) pending;  // functions reached by a call, still to lower
    Vec(bool) reached;      // by DefId, for functions that are not generic
    uint64_t *instances;    // the generic instances reached: open addressing of
    uint32_t instance_count, instance_cap;   // DefId << 32 | targs, 0 = empty
    bool too_deep;          // E0331 was reported
} Lowerer;

static inline LocalId air_local_of(LocalId ast_local) {
    return (LocalId){ast_local.id + 1};
}

static inline Operand op_copy(LocalId local, TypeId type) {
    return (Operand){.kind = OPERAND_COPY, .type = type, .as.local = local};
}

static inline Operand op_int(uint64_t value, TypeId type) {
    return (Operand){.kind = OPERAND_INT, .type = type, .as.int_value = value};
}

static inline Operand op_bool(bool value) {
    return (Operand){.kind = OPERAND_BOOL, .type = type_id(TID_BOOL), .as.bool_value = value};
}

static inline Operand op_unit(void) {
    return (Operand){.kind = OPERAND_UNIT, .type = type_id(TID_UNIT)};
}

// lower.c: blocks and statements
LocalId new_temp(Lowerer *l, TypeId type);
void emit_assign(Lowerer *l, LocalId dest, Rvalue rvalue);
LocalId emit_temp(Lowerer *l, TypeId type, Rvalue rvalue);
void emit_runtime(Lowerer *l, RuntimeFn fn, const Operand *args, uint32_t count, LocalId *dest);
void terminate(Lowerer *l, Terminator term);   // ends the block, continues in a new one
void emit_goto(Lowerer *l, BlockId target);
void switch_to(Lowerer *l, BlockId block);
void emit_assert(Lowerer *l, Operand fails, PanicKind panic, Span span);
Operand lower_block(Lowerer *l, Block *block, bool as_value);
// lower_stmt.c: the body of a loop; `latch` is where `continue` goes.
void lower_loop_body(Lowerer *l, Block *body, BlockId latch, BlockId exit);
LocalId to_local(Lowerer *l, Operand operand);   // a local holding the operand

// lower_expr.c
Operand lower_expr(Lowerer *l, Expr *e);
Operand emit_user_call(Lowerer *l, Expr *e, FnDecl *fn, Expr *receiver);
bool type_is_value(TypeId type);   // false for () and !

// mono.c: monomorphization (docs/design/compiler-architecture.md, 10)
// Queues `fn` with the type arguments `targs` (a tuple, `()` if it is not
// generic) to be lowered: only functions reachable from `main` are.
void reach(Lowerer *l, FnDecl *fn, TypeId targs);
// Substitutes the instance's type arguments into a lowered body, then
// reaches the functions it calls.
void mono_body(Lowerer *l, AirBody *body);
void mono_free(Lowerer *l);

// lower_ops.c
typedef struct {
    Op op;
    TypeId type;     // the type of both operands
    Operand a, b;
    Span span;       // where a failed check is reported
} Arith;

Operand lower_binary_op(Lowerer *l, Arith x);
Operand lower_unary(Lowerer *l, Expr *e);
Operand lower_cast(Lowerer *l, Operand value, TypeId to, Span span);
Operand lower_interpolation(Lowerer *l, Expr *e);
void lower_print(Lowerer *l, Expr *arg, bool newline);

// lower_method.c
Operand lower_method_call(Lowerer *l, Expr *call);
Operand lower_field(Lowerer *l, Expr *e);
Operand lower_index(Lowerer *l, Expr *e);
void lower_panic_assert(Lowerer *l, Expr *call, Builtin builtin);
void emit_bounds_check(Lowerer *l, Operand index, Operand len, Span span);

// lower_list.c: `List<T>` literals, indexing and loops
bool is_list(Lowerer *l, TypeId type);
Operand lower_list_literal(Lowerer *l, Expr *e);
Operand lower_list_repeat(Lowerer *l, Expr *e);
Place lower_list_index(Lowerer *l, Expr *e);   // the element's place, bounds checked
void lower_for_list(Lowerer *l, Stmt *s);

// lower_pattern.c
Operand build_variant(Lowerer *l, TypeId type, uint32_t index, ArgList args);
Operand variant_value(Lowerer *l, TypeId type, uint32_t index, const Operand *fields,
                      uint32_t count);
Operand is_variant(Lowerer *l, Place place, TypeId type, uint32_t index);   // a bool
Place variant_field(Lowerer *l, Place place, uint32_t index, uint32_t field);

// lower_option.c: `?` and `??`
Operand lower_try(Lowerer *l, Expr *e);
Operand lower_coalesce(Lowerer *l, Expr *e);
Operand enum_equality(Lowerer *l, Op op, Operand a, Operand b);
Operand lower_tuple(Lowerer *l, Expr *e);
// Tests `p` against the value at `place`: jumps to `fail` if it does not
// match, else continues in the current block with the bindings assigned.
void lower_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail);
Operand lower_match(Lowerer *l, Expr *e);

// lower_intrinsic.c
Operand lower_intrinsic(Lowerer *l, Expr *call, FnDecl *fn);

// lower_struct.c
Place lower_place(Lowerer *l, Expr *e);
Operand place_operand(Place place, OperandKind kind, TypeId type);
Operand lower_field_read(Lowerer *l, Expr *e);
Operand lower_struct_literal(Lowerer *l, Expr *e);
Operand lower_constructor(Lowerer *l, Expr *call);
void emit_store(Lowerer *l, Place place, Operand value);
// Reads a variable now when `next`, evaluated after it, could assign to it
// (a block or an `if`): evaluation is left to right (spec 4.10).
Operand settle(Lowerer *l, Operand value, const Expr *next);

#endif
