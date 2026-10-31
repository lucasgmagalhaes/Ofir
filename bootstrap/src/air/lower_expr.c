// Lowering of expressions to operands.

#include "air/lower_internal.h"
#include "sema/const_eval.h"

bool type_is_value(TypeId type) {
    return type.id != TID_UNIT && type.id != TID_NEVER && type.id != TID_ERROR;
}

static Operand int_literal(Lowerer *l, Expr *e) {
    if (type_get(l->types, e->type)->kind == TY_FLOAT) {   // `var x: float = 0`
        return (Operand){.kind = OPERAND_FLOAT, .type = e->type,
                         .as.float_value = (double)e->as.int_value};
    }
    return op_int(e->as.int_value, e->type);
}

static Operand const_operand(const ConstValue *v, TypeId type) {
    switch (v->kind) {
    case CV_INT: return op_int(v->bits, type);
    case CV_FLOAT: return (Operand){.kind = OPERAND_FLOAT, .type = type, .as.float_value = v->f};
    case CV_BOOL: return op_bool(v->b);
    case CV_STR: return (Operand){.kind = OPERAND_STR, .type = type, .as.str = v->s};
    }
    return op_unit();
}

static Operand def_value(Lowerer *l, Expr *e, Def *def) {
    if (def->kind == DEF_CONST) {   // evaluated by the checker (sema/const_eval.c)
        return const_operand(def->value, e->type);
    }
    diag_error(&l->sema->session->diags, "E0901", e->span,
               "global variables are not supported yet");
    return op_unit();
}

static Operand name(Lowerer *l, Expr *e) {
    Res res = e->as.ident.res;
    if (res.kind == RES_LOCAL) {
        return op_copy(air_local_of((LocalId){res.index}), e->type);
    }
    return def_value(l, e, sema_def(l->sema, (DefId){res.index}));
}

static Operand call(Lowerer *l, Expr *e) {
    Expr *callee = e->as.call.callee;
    if (callee->kind == EXPR_FIELD) {
        return lower_method_call(l, e);
    }
    Def *def = sema_def(l->sema, (DefId){callee->as.ident.res.index});
    ArgList args = e->as.call.args;
    if (def->kind == DEF_STRUCT) {
        return lower_constructor(l, e);
    }
    if (def->kind == DEF_BUILTIN) {
        if (def->builtin == BUILTIN_PANIC || def->builtin == BUILTIN_ASSERT) {
            lower_panic_assert(l, e, def->builtin);
        } else {
            lower_print(l, args.items[0].value, def->builtin == BUILTIN_PRINTLN);
        }
        return op_unit();
    }
    AirStmt s = {.kind = AIR_CALL};
    s.as.call.fn = (DefId){callee->as.ident.res.index};
    s.as.call.arg_count = args.len;
    s.as.call.args = ARENA_ARRAY(&l->sema->session->arena, Operand, args.len + 1);
    for (uint32_t i = 0; i < args.len; i++) {
        s.as.call.args[i] = lower_expr(l, args.items[i].value);
        if (i + 1 < args.len) {
            s.as.call.args[i] = settle(l, s.as.call.args[i], args.items[i + 1].value);
        }
    }
    Operand result = op_unit();
    if (type_is_value(e->type)) {
        s.dest = new_temp(l, e->type);
        s.has_dest = true;
        result = op_copy(s.dest, e->type);
    }
    air_push(l->body, l->block, s);
    if (e->type.id == TID_NEVER) {
        terminate(l, (Terminator){.kind = TERM_UNREACHABLE});
    }
    return result;
}

// Assigns `value` to `dest` when the expression has a value.
static void store(Lowerer *l, LocalId dest, bool has_dest, Operand value) {
    if (has_dest && value.kind != OPERAND_UNIT) {
        emit_assign(l, dest, (Rvalue){.kind = RV_USE, .a = value});
    }
}

static Operand lower_if(Lowerer *l, Expr *e) {
    bool has_value = type_is_value(e->type);
    LocalId dest = has_value ? new_temp(l, e->type) : (LocalId){0};
    Operand cond = lower_expr(l, e->as.if_.cond);
    BlockId then = air_new_block(l->body), other = air_new_block(l->body);
    BlockId join = air_new_block(l->body);
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = cond, .target = then, .other = other});
    switch_to(l, then);
    store(l, dest, has_value, lower_block(l, e->as.if_.then, has_value));
    emit_goto(l, join);
    switch_to(l, other);
    Expr *else_ = e->as.if_.else_;
    if (else_) {
        Operand value = else_->kind == EXPR_IF ? lower_if(l, else_)
                                               : lower_block(l, else_->as.block, has_value);
        store(l, dest, has_value, value);
    }
    emit_goto(l, join);
    switch_to(l, join);
    return has_value ? op_copy(dest, e->type) : op_unit();
}

// `a && b` and `a || b` evaluate `b` only when needed.
static Operand logical(Lowerer *l, Expr *e) {
    bool is_and = e->as.binary.op == OP_LOGICAL_AND;
    LocalId dest = new_temp(l, type_id(TID_BOOL));
    emit_assign(l, dest, (Rvalue){.kind = RV_USE, .a = lower_expr(l, e->as.binary.lhs)});
    BlockId rhs = air_new_block(l->body), join = air_new_block(l->body);
    Operand lhs = op_copy(dest, type_id(TID_BOOL));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = lhs, .target = is_and ? rhs : join,
                              .other = is_and ? join : rhs});
    switch_to(l, rhs);
    emit_assign(l, dest, (Rvalue){.kind = RV_USE, .a = lower_expr(l, e->as.binary.rhs)});
    emit_goto(l, join);
    switch_to(l, join);
    return lhs;
}

static Operand binary(Lowerer *l, Expr *e) {
    Op op = e->as.binary.op;
    if (op == OP_LOGICAL_AND || op == OP_LOGICAL_OR) {
        return logical(l, e);
    }
    Operand a = settle(l, lower_expr(l, e->as.binary.lhs), e->as.binary.rhs);
    Operand b = lower_expr(l, e->as.binary.rhs);
    return lower_binary_op(l, (Arith){.op = op, .type = e->as.binary.lhs->type, .a = a, .b = b,
                                      .span = e->span});
}

// check-sizes: table
Operand lower_expr(Lowerer *l, Expr *e) {
    switch (e->kind) {
    case EXPR_INT: return int_literal(l, e);
    case EXPR_FLOAT:
        return (Operand){.kind = OPERAND_FLOAT, .type = e->type,
                         .as.float_value = e->as.float_value};
    case EXPR_BYTE: return op_int(e->as.byte_value, e->type);
    case EXPR_BOOL: return op_bool(e->as.bool_value);
    case EXPR_STR: return (Operand){.kind = OPERAND_STR, .type = e->type, .as.str = e->as.str};
    case EXPR_INTERP: return lower_interpolation(l, e);
    case EXPR_IDENT: case EXPR_SELF: return name(l, e);
    case EXPR_UNARY: return lower_unary(l, e);
    case EXPR_BINARY: return binary(l, e);
    case EXPR_CAST: return lower_cast(l, lower_expr(l, e->as.cast.value), e->type, e->span);
    case EXPR_CALL: return call(l, e);
    case EXPR_FIELD:
        return e->as.field.res.kind == RES_FIELD ? lower_field_read(l, e) : lower_field(l, e);
    case EXPR_TUPLE_FIELD: return lower_field_read(l, e);
    case EXPR_STRUCT: return lower_struct_literal(l, e);
    case EXPR_INDEX: return lower_index(l, e);
    case EXPR_IF: return lower_if(l, e);
    case EXPR_BLOCK: case EXPR_UNSAFE:
        return lower_block(l, e->as.block, type_is_value(e->type));
    default:
        return op_unit();   // () and the constructs the checker rejects
    }
}
