// Lowering of the built-in methods and constants of the primitive types,
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

static Operand select_value(Lowerer *l, Operand cond, Operand a, Operand b) {
    return value_of(l, a.type, (Rvalue){.kind = RV_SELECT, .cond = cond, .a = a, .b = b});
}

// The place a `mut self` method modifies: the receiver itself, not a copy.
static Operand receiver_place(Lowerer *l, Expr *base) {
    return place_operand(lower_place(l, base), OPERAND_ADDR, base->type);
}

// A runtime call writing its str result through the first argument.
static Operand str_result(Lowerer *l, RuntimeFn fn, Operand *args, uint32_t count) {
    LocalId dest = new_temp(l, type_id(TID_STR));
    args[0] = (Operand){.kind = OPERAND_ADDR, .type = type_id(TID_STR), .as.local = dest};
    emit_runtime(l, fn, args, count, NULL);
    return op_copy(dest, type_id(TID_STR));
}

static Operand runtime_bool(Lowerer *l, RuntimeFn fn, Operand a, Operand b) {
    Operand args[] = {address(l, a), address(l, b)};
    LocalId dest = new_temp(l, type_id(TID_BOOL));
    emit_runtime(l, fn, args, 2, &dest);
    return op_copy(dest, type_id(TID_BOOL));
}

// base.pow(exp) by squaring; each multiplication is checked, and the base
// is squared only while bits of the exponent remain.
static Operand power(Lowerer *l, Operand base, Operand exp, Span span) {
    TypeId type = base.type, u32 = type_id(TID_U32), boolean = type_id(TID_BOOL);
    LocalId result = emit_temp(l, type, (Rvalue){.kind = RV_USE, .a = op_int(1, type)});
    LocalId b = emit_temp(l, type, (Rvalue){.kind = RV_USE, .a = base});
    LocalId e = emit_temp(l, u32, (Rvalue){.kind = RV_USE, .a = exp});
    BlockId header = air_new_block(l->body), body = air_new_block(l->body);
    BlockId odd = air_new_block(l->body), even = air_new_block(l->body);
    BlockId square = air_new_block(l->body), exit = air_new_block(l->body);
    emit_goto(l, header);
    switch_to(l, header);
    Operand more = binary(l, OP_NE, boolean, op_copy(e, u32), op_int(0, u32));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = more, .target = body, .other = exit});
    switch_to(l, body);
    Operand bit = binary(l, OP_AND, u32, op_copy(e, u32), op_int(1, u32));
    Operand is_odd = binary(l, OP_NE, boolean, bit, op_int(0, u32));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = is_odd, .target = odd, .other = even});
    switch_to(l, odd);
    Arith mul = {.op = OP_MUL, .type = type, .a = op_copy(result, type), .b = op_copy(b, type),
                 .span = span};
    emit_assign(l, result, (Rvalue){.kind = RV_USE, .a = lower_binary_op(l, mul)});
    emit_goto(l, even);
    switch_to(l, even);
    emit_assign(l, e, (Rvalue){.kind = RV_BINARY, .op = OP_SHR, .a = op_copy(e, u32),
                               .b = op_int(1, u32)});
    Operand again = binary(l, OP_NE, boolean, op_copy(e, u32), op_int(0, u32));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = again, .target = square,
                              .other = exit});
    switch_to(l, square);
    Arith sq = {.op = OP_MUL, .type = type, .a = op_copy(b, type), .b = op_copy(b, type),
                .span = span};
    emit_assign(l, b, (Rvalue){.kind = RV_USE, .a = lower_binary_op(l, sq)});
    emit_goto(l, header);
    switch_to(l, exit);
    return op_copy(result, type);
}

static Operand integer_method(Lowerer *l, BuiltinMethod m, Expr *call, Operand self,
                              Operand *args) {
    TypeId type = self.type;
    bool is_signed = type_get(l->types, type)->data <= INT_I64;
    TypeId boolean = type_id(TID_BOOL);
    switch (m) {
    case BM_WRAPPING_ADD: return binary(l, OP_ADD, type, self, args[0]);
    case BM_WRAPPING_SUB: return binary(l, OP_SUB, type, self, args[0]);
    case BM_WRAPPING_MUL: return binary(l, OP_MUL, type, self, args[0]);
    case BM_ABS: {
        if (!is_signed) {
            return self;
        }
        Arith neg = {.op = OP_SUB, .type = type, .a = op_int(0, type), .b = self,
                     .span = call->span};   // overflows only for MIN
        Operand negative = binary(l, OP_LT, boolean, self, op_int(0, type));
        return select_value(l, negative, lower_binary_op(l, neg), self);
    }
    case BM_MIN: return select_value(l, binary(l, OP_LT, boolean, args[0], self), args[0], self);
    case BM_MAX: return select_value(l, binary(l, OP_GT, boolean, args[0], self), args[0], self);
    case BM_CLAMP: {
        Operand low = select_value(l, binary(l, OP_LT, boolean, self, args[0]), args[0], self);
        return select_value(l, binary(l, OP_GT, boolean, low, args[1]), args[1], low);
    }
    case BM_POW: return power(l, self, args[0], call->span);
    case BM_WRAPPING_CAST: return value_of(l, call->type, (Rvalue){.kind = RV_CAST, .a = self,
                                                                   .to = call->type});
    case BM_SATURATING_CAST:
        return value_of(l, call->type, (Rvalue){.kind = RV_SATURATE, .a = self, .to = call->type});
    default: return op_unit();
    }
}

static Operand float_fn(Lowerer *l, FloatFn fn, Operand a, Operand b) {
    return value_of(l, a.type, (Rvalue){.kind = RV_FLOAT_FN, .fn = fn, .a = a, .b = b});
}

static Operand float_method(Lowerer *l, BuiltinMethod m, Operand self, Operand *args) {
    switch (m) {
    case BM_SQRT: return float_fn(l, FF_SQRT, self, op_unit());
    case BM_FABS: return float_fn(l, FF_FABS, self, op_unit());
    case BM_FLOOR: return float_fn(l, FF_FLOOR, self, op_unit());
    case BM_CEIL: return float_fn(l, FF_CEIL, self, op_unit());
    case BM_FMIN: return float_fn(l, FF_MINNUM, self, args[0]);
    case BM_FMAX: return float_fn(l, FF_MAXNUM, self, args[0]);
    case BM_IS_NAN: return binary(l, OP_NE, type_id(TID_BOOL), self, self);   // NaN != NaN
    default: return op_unit();
    }
}

// check-sizes: table
static Operand str_method(Lowerer *l, BuiltinMethod m, Expr *base, Operand self, Operand *args) {
    TypeId boolean = type_id(TID_BOOL);
    switch (m) {
    case BM_STR_LEN: return value_of(l, type_id(TID_INT), (Rvalue){.kind = RV_STR_LEN, .a = self});
    case BM_STR_IS_EMPTY: {
        Operand len = value_of(l, type_id(TID_INT), (Rvalue){.kind = RV_STR_LEN, .a = self});
        return binary(l, OP_EQ, boolean, len, op_int(0, type_id(TID_INT)));
    }
    case BM_STR_PUSH: {
        Operand call_args[] = {receiver_place(l, base), address(l, args[0])};
        emit_runtime(l, RT_STR_PUSH_STR, call_args, 2, NULL);
        return op_unit();
    }
    case BM_STR_CLEAR: {
        Operand place = receiver_place(l, base);
        emit_runtime(l, RT_STR_CLEAR, &place, 1, NULL);
        return op_unit();
    }
    case BM_STR_STARTS_WITH: return runtime_bool(l, RT_STR_STARTS_WITH, self, args[0]);
    case BM_STR_ENDS_WITH: return runtime_bool(l, RT_STR_ENDS_WITH, self, args[0]);
    case BM_STR_CONTAINS: return runtime_bool(l, RT_STR_CONTAINS, self, args[0]);
    case BM_STR_REPEAT: {
        Operand call_args[] = {op_unit(), address(l, self), args[0]};
        return str_result(l, RT_STR_REPEAT, call_args, 3);
    }
    case BM_STR_TO_LOWER: case BM_STR_TO_UPPER: case BM_STR_TO_OWNED: {
        RuntimeFn fn = m == BM_STR_TO_LOWER   ? RT_STR_TO_LOWER
                       : m == BM_STR_TO_UPPER ? RT_STR_TO_UPPER
                                              : RT_STR_TO_OWNED;
        Operand call_args[] = {op_unit(), address(l, self)};
        return str_result(l, fn, call_args, 2);
    }
    default: return op_unit();
    }
}

Operand lower_method_call(Lowerer *l, Expr *call) {
    Expr *field = call->as.call.callee;
    if (field->as.field.res.kind == RES_VARIANT) {   // `Shape.Circle(1.0)`
        return build_variant(l, call->type, field->as.field.res.index, call->as.call.args);
    }
    if (field->as.field.res.kind == RES_DEF) {   // a method declared in a struct
        FnDecl *fn = sema_def(l->sema, (DefId){field->as.field.res.index})->fn;
        return emit_user_call(l, call, fn, fn->has_self ? field->as.field.base : NULL);
    }
    BuiltinMethod m = (BuiltinMethod)field->as.field.res.index;
    if (m == BM_STR_NEW) {
        return (Operand){.kind = OPERAND_STR, .type = type_id(TID_STR), .as.str = SV("")};
    }
    const BuiltinInfo *info = builtin_info(m);
    Expr *base = field->as.field.base;
    // A `mut self` method works on the receiver's place, not on a copy.
    Operand self = info->self_mode == MODE_MUT ? op_unit() : lower_expr(l, base);
    Operand args[2] = {op_unit(), op_unit()};
    for (uint32_t i = 0; i < call->as.call.args.len && i < 2; i++) {
        args[i] = lower_expr(l, call->as.call.args.items[i].value);
    }
    switch (info->receiver) {
    case RECV_INT: return integer_method(l, m, call, self, args);
    case RECV_FLOAT: return float_method(l, m, self, args);
    default: return str_method(l, m, base, self, args);
    }
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
