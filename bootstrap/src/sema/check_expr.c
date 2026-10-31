// Type checking of expressions (spec 2 and 4.8).

#include "sema/check_internal.h"

static StrView def_name(Checker *c, const Def *def) {
    return symbol_name(&c->sema->session->names, def->name);
}

// ---------------------------------------------------------------------------
// Literals

static bool int_fits(const Ty *ty, uint64_t value, bool negative) {
    static const uint64_t MAX[] = {
        [INT_I8] = INT8_MAX, [INT_I16] = INT16_MAX, [INT_I32] = INT32_MAX, [INT_I64] = INT64_MAX,
        [INT_U8] = UINT8_MAX, [INT_U16] = UINT16_MAX, [INT_U32] = UINT32_MAX,
        [INT_U64] = UINT64_MAX,
    };
    if (negative) {   // negating an unsigned value is reported by the `-` operator
        return ty->data > INT_I64 || value <= MAX[ty->data] + 1;
    }
    return value <= MAX[ty->data];
}

// An integer literal takes the expected integer or float type, else `int`.
static TypeId int_literal(Checker *c, Expr *e, TypeId expected, bool negative) {
    const Ty *ty = type_get(c->types, expected);
    TypeId type = ty->kind == TY_INT || ty->kind == TY_FLOAT ? expected : type_id(TID_INT);
    ty = type_get(c->types, type);
    if (ty->kind == TY_INT && !int_fits(ty, e->as.int_value, negative)) {
        check_error(c, "E0312", e->span, "integer literal out of range for `%s`",
                    type_name(c, type));
    }
    return type;
}

static TypeId float_literal(Checker *c, TypeId expected) {
    return type_get(c->types, expected)->kind == TY_FLOAT ? expected : type_id(TID_FLOAT);
}

static TypeId interpolation(Checker *c, Expr *e) {
    for (uint32_t i = 0; i < e->as.parts.len; i++) {
        Expr *part = e->as.parts.items[i];
        TypeId type = check_expr(c, part, NO_TYPE);
        TyKind kind = type_get(c->types, type)->kind;
        bool printable = kind == TY_INT || kind == TY_FLOAT || kind == TY_BOOL ||
                         kind == TY_STR || kind == TY_ERROR;
        if (!printable) {
            check_error(c, "E0314", part->span,
                        "a value of type `%s` cannot be interpolated into a string",
                        type_name(c, type));
        }
    }
    return type_id(TID_STR);
}

// ---------------------------------------------------------------------------
// Names

static TypeId def_value(Checker *c, Def *def, Span span) {
    switch (def->kind) {
    case DEF_CONST: case DEF_GLOBAL: return check_const(c, def);
    case DEF_FN: case DEF_BUILTIN: return unsupported(c, span, "function values are");
    default:
        check_error(c, "E0310", span, "expected a value, found type `" SV_FMT "`",
                    SV_ARG(def_name(c, def)));
        return type_id(TID_ERROR);
    }
}

static TypeId name_value(Checker *c, Res res, Span span) {
    switch (res.kind) {
    case RES_LOCAL: return c->local_types[res.index];
    case RES_DEF: return def_value(c, sema_def(c->sema, (DefId){res.index}), span);
    default: return type_id(TID_ERROR);
    }
}

// ---------------------------------------------------------------------------
// Operators

static bool is_literal(const Expr *e) {
    if (e->kind == EXPR_UNARY && e->as.unary.op == OP_NEG) {
        e = e->as.unary.operand;
    }
    return e->kind == EXPR_INT || e->kind == EXPR_FLOAT;
}

static TypeId operator_error(Checker *c, Span span, TypeId type) {
    check_error(c, "E0313", span, "operator cannot be applied to type `%s`", type_name(c, type));
    return type_id(TID_ERROR);
}

static TypeId unary(Checker *c, Expr *e, TypeId expected) {
    Expr *operand = e->as.unary.operand;
    Op op = e->as.unary.op;
    if (op == OP_DEREF || op == OP_REF || op == OP_REF_MUT) {
        return unsupported(c, e->span, "references are");
    }
    TypeId type = op == OP_NEG && operand->kind == EXPR_INT
                      ? int_literal(c, operand, expected, true)
                      : check_expr(c, operand, expected);
    operand->type = type;
    const Ty *ty = type_get(c->types, type);
    bool ok = op == OP_NEG ? ty->kind == TY_FLOAT || (ty->kind == TY_INT && ty->data <= INT_I64)
                           : ty->kind == TY_BOOL || ty->kind == TY_INT;
    return ok || ty->kind == TY_ERROR ? type : operator_error(c, e->span, type);
}

// Which operand types an operator accepts.
static bool operand_ok(const Ty *ty, Op op) {
    switch (op) {
    case OP_ADD: return ty->kind == TY_INT || ty->kind == TY_FLOAT || ty->kind == TY_STR;
    case OP_SUB: case OP_MUL: case OP_DIV: case OP_REM:
        return ty->kind == TY_INT || ty->kind == TY_FLOAT;
    case OP_AND: case OP_OR: case OP_XOR: case OP_SHL: case OP_SHR: return ty->kind == TY_INT;
    case OP_EQ: case OP_NE: return ty->kind <= TY_STR;   // the primitive types and ()
    case OP_LT: case OP_LE: case OP_GT: case OP_GE:
        return ty->kind == TY_INT || ty->kind == TY_FLOAT || ty->kind == TY_STR;
    case OP_LOGICAL_AND: case OP_LOGICAL_OR: return ty->kind == TY_BOOL;
    default: return false;
    }
}

// Both operands have the same type. A literal on the left takes the type of
// the right operand: `1 + x` with `x: u8` is a `u8` addition.
static TypeId binary(Checker *c, Expr *e, TypeId expected) {
    Op op = e->as.binary.op;
    if (op == OP_COALESCE) {
        return unsupported(c, e->span, "the `??` operator is");
    }
    bool compares = op >= OP_EQ && op <= OP_GE;
    TypeId hint = compares || op == OP_LOGICAL_AND || op == OP_LOGICAL_OR ? NO_TYPE : expected;
    Expr *first = e->as.binary.lhs, *second = e->as.binary.rhs;
    if (is_literal(first) && !is_literal(second)) {
        first = e->as.binary.rhs;
        second = e->as.binary.lhs;
    }
    TypeId type = check_expr(c, first, hint);
    coerce(c, second, type);
    DefId id;
    const EnumInfo *info = (op == OP_EQ || op == OP_NE) ? enum_of(c, type, &id) : NULL;
    bool fieldless = info && !info->has_payload;   // enums without payloads compare tags
    if (!fieldless && !operand_ok(type_get(c->types, type), op) && type.id != TID_ERROR) {
        return operator_error(c, e->span, type);
    }
    return compares ? type_id(TID_BOOL) : type;
}

static TypeId cast(Checker *c, Expr *e) {
    TypeId from = check_expr(c, e->as.cast.value, NO_TYPE);
    TypeId to = resolve_type(c, e->as.cast.type);
    bool ok = (type_is_numeric(c->types, from) && type_is_numeric(c->types, to)) ||
              type_eq(from, to) || from.id == TID_ERROR || to.id == TID_ERROR;
    if (!ok) {
        char from_name[64];
        snprintf(from_name, sizeof from_name, "%s", type_name(c, from));
        check_error(c, "E0313", e->span, "cannot cast `%s` to `%s`", from_name,
                    type_name(c, to));
    }
    return to;
}

// ---------------------------------------------------------------------------
// Calls

TypeId check_args(Checker *c, Expr *call, const Ty *sig) {
    ArgList args = call->as.call.args;
    if (args.len != sig->arg_count) {
        check_error(c, "E0302", call->span,
                    "wrong number of arguments: expected %u, found %u", sig->arg_count, args.len);
    }
    for (uint32_t i = 0; i < args.len; i++) {
        if (args.items[i].name.id != 0) {
            unsupported(c, args.items[i].value->span, "named arguments are");
        }
        coerce(c, args.items[i].value, i < sig->arg_count ? sig->args[i] : NO_TYPE);
    }
    return sig->inner;
}

// print(str), println(str), panic(str) ! and assert(bool).
static TypeId builtin_call(Checker *c, Expr *call, Builtin builtin) {
    TypeId param = type_id(builtin == BUILTIN_ASSERT ? TID_BOOL : TID_STR);
    TypeId ret = type_id(builtin == BUILTIN_PANIC ? TID_NEVER : TID_UNIT);
    Ty sig = {.kind = TY_FN, .inner = ret, .arg_count = 1, .args = &param};
    return check_args(c, call, &sig);
}

static TypeId check_fn_call(Checker *c, Expr *e, const Def *def, const Ty *sig) {
    TypeId ret = check_args(c, e, sig);
    if (def->kind == DEF_FN) {
        check_mut_args(c, def->fn, e);
    }
    return ret;
}

static TypeId call(Checker *c, Expr *e) {
    Expr *callee = e->as.call.callee;
    if (callee->kind == EXPR_FIELD) {
        return check_method_call(c, e);
    }
    TypeId type;
    if (callee->kind == EXPR_IDENT && callee->as.ident.res.kind == RES_DEF) {
        Def *def = sema_def(c->sema, (DefId){callee->as.ident.res.index});
        if (def->kind == DEF_BUILTIN) {
            return builtin_call(c, e, def->builtin);
        }
        if (def->kind == DEF_STRUCT) {
            callee->type = type_id(TID_UNIT);
            return check_constructor(c, e, (DefId){callee->as.ident.res.index});
        }
        type = def->kind == DEF_FN ? fn_signature(c, def->fn) : def_value(c, def, callee->span);
    } else {
        type = check_expr(c, callee, NO_TYPE);
    }
    callee->type = type;
    const Ty *sig = type_get(c->types, type);
    if (sig->kind == TY_FN && callee->kind == EXPR_IDENT && callee->as.ident.res.kind == RES_DEF) {
        return check_fn_call(c, e, sema_def(c->sema, (DefId){callee->as.ident.res.index}), sig);
    }
    if (sig->kind != TY_FN) {
        if (sig->kind != TY_ERROR) {
            check_error(c, "E0311", callee->span, "expression of type `%s` is not a function",
                        type_name(c, type));
        }
        for (uint32_t i = 0; i < e->as.call.args.len; i++) {
            check_expr(c, e->as.call.args.items[i].value, NO_TYPE);
        }
        return type_id(TID_ERROR);
    }
    return check_args(c, e, sig);
}

// ---------------------------------------------------------------------------

// `(a, b)`: each element takes the matching element of the expected type.
static TypeId tuple(Checker *c, Expr *e, TypeId expected) {
    uint32_t count = e->as.elems.len;
    const Ty *hint = type_get(c->types, expected);
    TypeId *types = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        bool hinted = hint->kind == TY_TUPLE && hint->arg_count == count;
        types[i] = check_expr(c, e->as.elems.items[i], hinted ? hint->args[i] : NO_TYPE);
    }
    return type_tuple(c->types, types, count);
}

// check-sizes: table
static TypeId synthesize(Checker *c, Expr *e, TypeId expected) {
    switch (e->kind) {
    case EXPR_INT: return int_literal(c, e, expected, false);
    case EXPR_FLOAT: return float_literal(c, expected);
    case EXPR_BYTE: return type_id(TID_U8);
    case EXPR_BOOL: return type_id(TID_BOOL);
    case EXPR_STR: return type_id(TID_STR);
    case EXPR_INTERP: return interpolation(c, e);
    case EXPR_IDENT: case EXPR_SELF: return name_value(c, e->as.ident.res, e->span);
    case EXPR_UNARY: return unary(c, e, expected);
    case EXPR_BINARY: return binary(c, e, expected);
    case EXPR_CAST: return cast(c, e);
    case EXPR_CALL: return call(c, e);
    case EXPR_IF: return check_if(c, e, expected, true);
    case EXPR_BLOCK: case EXPR_UNSAFE: return check_block(c, e->as.block, expected, true);
    case EXPR_TUPLE: return tuple(c, e, expected);
    case EXPR_SELF_TYPE: return type_id(TID_ERROR);   // replaced by the resolver
    case EXPR_GENERIC: return unsupported(c, e->span, "generic types are");
    case EXPR_RANGE: return unsupported(c, e->span, "ranges outside of `for` are");
    case EXPR_INDEX: return check_index(c, e);
    case EXPR_FIELD: return check_field(c, e);
    case EXPR_TUPLE_FIELD: return check_tuple_field(c, e);
    case EXPR_TRY: return unsupported(c, e->span, "the `?` operator is");
    case EXPR_STRUCT: return check_struct_literal(c, e);
    case EXPR_ARRAY: case EXPR_ARRAY_REPEAT: return unsupported(c, e->span, "arrays are");
    case EXPR_VARIANT: return check_variant_shorthand(c, e, expected);
    case EXPR_MATCH: return check_match(c, e, expected, true);
    }
    return type_id(TID_ERROR);
}

TypeId check_expr(Checker *c, Expr *e, TypeId expected) {
    e->type = synthesize(c, e, expected);
    return e->type;
}

TypeId coerce(Checker *c, Expr *e, TypeId expected) {
    TypeId found = check_expr(c, e, expected);
    if (!types_compatible(found, expected)) {
        char expected_name[64];
        snprintf(expected_name, sizeof expected_name, "%s", type_name(c, expected));
        check_error(c, "E0300", e->span, "mismatched types: expected %s, found %s",
                    expected_name, type_name(c, found));
    }
    return found;
}

TypeId check_const(Checker *c, Def *def) {
    if (def->typed) {
        return def->type;
    }
    if (def->checking) {
        check_error(c, "E0800", def->span, "the constant `" SV_FMT "` depends on itself",
                    SV_ARG(def_name(c, def)));
        return type_id(TID_ERROR);
    }
    def->checking = true;
    uint32_t saved_module = sema_enter(c->sema, def->module);
    TypeExpr *annotation = def->item->as.const_.type;
    Expr *value = def->item->as.const_.value;
    TypeId type = annotation ? resolve_type(c, annotation) : NO_TYPE;
    TypeId found = annotation ? coerce(c, value, type) : check_expr(c, value, NO_TYPE);
    def->type = annotation ? type : found;
    if (def->kind == DEF_CONST) {
        def->value = const_eval(c, value);   // while `checking` is set: catches cycles
    }
    sema_enter(c->sema, saved_module);
    def->checking = false;
    def->typed = true;
    return def->type;
}
