// Lowering of method calls, the built-in constants of the primitive types,
// str indexing, panic and assert.

#include "air/lower_internal.h"
#include "sema/builtins.h"

#include <float.h>
#include <math.h>

static const uint32_t INT_BITS[] = {8, 16, 32, 64, 8, 16, 32, 64};

static Operand value_of(Lowerer *l, TypeId type, Rvalue rv) {
    return op_copy(emit_temp(l, type, rv), type);
}

static Operand address(Lowerer *l, Operand value) {
    return (Operand){.kind = OPERAND_ADDR, .type = value.type, .as.local = to_local(l, value)};
}

static Operand binary(Lowerer *l, Op op, TypeId type, Operand a, Operand b) {
    return value_of(l, type, (Rvalue){.kind = RV_BINARY, .op = op, .a = a, .b = b});
}

// The methods still built in: `x.wrappingCast<T>()`, `x.saturatingCast<T>()`
// and `p.add(n)`. The others are in std (ADR 0019).
static Operand builtin_call(Lowerer *l, Expr *call) {
    Expr *field = call->as.call.callee;
    Operand self = lower_expr(l, field->as.field.base);
    switch ((BuiltinMethod)field->as.field.res.index) {
    case BM_WRAPPING_CAST:
        return value_of(l, call->type, (Rvalue){.kind = RV_CAST, .a = self, .to = call->type});
    case BM_SATURATING_CAST:
        return value_of(l, call->type, (Rvalue){.kind = RV_SATURATE, .a = self, .to = call->type});
    default: {
        Operand offset = lower_expr(l, call->as.call.args.items[0].value);
        return value_of(l, self.type, (Rvalue){.kind = RV_PTR_ADD, .a = self, .b = offset});
    }
    }
}

Operand lower_method_call(Lowerer *l, Expr *call) {
    Expr *field = call->as.call.callee;
    if (field->as.field.res.kind == RES_VARIANT) {   // `Shape.Circle(1.0)`
        return build_variant(l, call->type, field->as.field.res.index, call->as.call.args);
    }
    if (field->as.field.res.kind == RES_DEF) {   // a method declared in a type
        FnDecl *fn = sema_def(l->sema, (DefId){field->as.field.res.index})->fn;
        return emit_user_call(l, call, fn, fn->has_self ? field->as.field.base : NULL);
    }
    return builtin_call(l, call);
}

// `I.MIN`, `I.MAX`, `F.NAN`, `F.INFINITY`, `F.EPSILON`.
Operand lower_field(Lowerer *l, Expr *e) {
    const Ty *ty = type_get(l->types, e->type);
    BuiltinConst which = builtin_const(&l->sema->session->names, ty, e->as.field.name);
    if (ty->kind == TY_INT) {
        uint32_t bits = INT_BITS[ty->data];
        bool is_signed = ty->data <= INT_I64;
        uint64_t max = bits == 64 ? UINT64_MAX : (1ull << bits) - 1;
        uint64_t value = which == BC_MAX ? (is_signed ? max >> 1 : max)
                                         : (is_signed ? 1ull << (bits - 1) : 0);
        return op_int(value, e->type);
    }
    bool is_f32 = ty->data == 32;
    double value = which == BC_NAN        ? (double)NAN
                   : which == BC_INFINITY ? (double)INFINITY
                   : is_f32               ? (double)FLT_EPSILON
                                          : DBL_EPSILON;
    return (Operand){.kind = OPERAND_FLOAT, .type = e->type, .as.float_value = value};
}

// `s[i]`: the index is compared as unsigned, so a negative one is out of
// range too (spec 2.1).
Operand lower_index(Lowerer *l, Expr *e) {
    Operand s = lower_expr(l, e->as.index.base);
    Operand index = op_copy(to_local(l, lower_expr(l, e->as.index.index)), type_id(TID_INT));
    TypeId u64 = type_id(TID_U64), boolean = type_id(TID_BOOL);
    Operand len = value_of(l, type_id(TID_INT), (Rvalue){.kind = RV_STR_LEN, .a = s});
    Operand i_u = value_of(l, u64, (Rvalue){.kind = RV_CAST, .a = index, .to = u64});
    Operand len_u = value_of(l, u64, (Rvalue){.kind = RV_CAST, .a = len, .to = u64});
    Operand outside = binary(l, OP_GE, boolean, i_u, len_u);
    BlockId next = air_new_block(l->body);
    air_terminate(l->body, l->block, (Terminator){.kind = TERM_ASSERT, .cond = outside,
                                                  .target = next, .panic = PANIC_BOUNDS,
                                                  .index = index, .len = len, .span = e->span});
    switch_to(l, next);
    return value_of(l, type_id(TID_U8), (Rvalue){.kind = RV_STR_BYTE, .a = s, .b = index});
}

// panic(msg) ends the block; assert(cond) panics when cond is false.
void lower_panic_assert(Lowerer *l, Expr *call, Builtin builtin) {
    Operand value = lower_expr(l, call->as.call.args.items[0].value);
    if (builtin == BUILTIN_PANIC) {
        terminate(l, (Terminator){.kind = TERM_PANIC, .cond = address(l, value),
                                  .span = call->span});
        return;
    }
    Operand fails = value_of(l, type_id(TID_BOOL), (Rvalue){.kind = RV_UNARY, .op = OP_NOT,
                                                             .a = value});
    emit_assert(l, fails, PANIC_ASSERT, call->span);
}
