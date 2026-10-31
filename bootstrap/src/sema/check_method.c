// Type checking of method calls, fields and indexing. Stage 0 knows only
// the built-in methods of the primitive types (sema/builtins.c).

#include "sema/builtins.h"
#include "sema/check_internal.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

// The primitive type named by `e` (as in `str.new()` or `i32.MAX`), or NULL.
static const Def *primitive_name(Checker *c, const Expr *e) {
    if (e->kind != EXPR_IDENT || e->as.ident.res.kind != RES_DEF) {
        return NULL;
    }
    const Def *def = sema_def(c->sema, (DefId){e->as.ident.res.index});
    return def->kind == DEF_PRIMITIVE ? def : NULL;
}

void require_mutable(Checker *c, Expr *e) {
    bool ok = false;
    if ((e->kind == EXPR_IDENT || e->kind == EXPR_SELF) && e->as.ident.res.kind == RES_LOCAL) {
        ok = c->fn->locals.items[e->as.ident.res.index].mutable;
    } else if (e->kind == EXPR_IDENT && e->as.ident.res.kind == RES_DEF) {
        ok = sema_def(c->sema, (DefId){e->as.ident.res.index})->kind == DEF_GLOBAL;
    } else {
        ok = e->type.id == TID_ERROR;
    }
    if (!ok) {
        check_error(c, "E0315", e->span, "this value cannot be modified: declare it with `var`");
    }
}

static TypeId param_type(BuiltinType t, TypeId self, TypeId target) {
    switch (t) {
    case BT_SELF: return self;
    case BT_STR: return type_id(TID_STR);
    case BT_INT: return type_id(TID_INT);
    case BT_BOOL: return type_id(TID_BOOL);
    case BT_U32: return type_id(TID_U32);
    case BT_TARGET: return target;
    case BT_NONE: return type_id(TID_UNIT);
    }
    return type_id(TID_ERROR);
}

// The integer type argument of `x.wrappingCast<T>()`.
static TypeId type_argument(Checker *c, Expr *field, const BuiltinInfo *info) {
    TypeList args = field->as.field.type_args;
    if (args.len != (info->type_arg ? 1u : 0u)) {
        check_error(c, "E0302", field->span,
                    "wrong number of type arguments: expected %u, found %u",
                    info->type_arg ? 1u : 0u, args.len);
        return type_id(TID_ERROR);
    }
    if (!info->type_arg) {
        return type_id(TID_ERROR);
    }
    TypeId target = resolve_type(c, args.items[0]);
    if (!type_is_integer(c->types, target) && target.id != TID_ERROR) {
        check_error(c, "E0300", args.items[0]->span,
                    "mismatched types: expected an integer type, found %s", type_name(c, target));
        return type_id(TID_ERROR);
    }
    return target;
}

static TypeId call_builtin(Checker *c, Expr *call, const BuiltinInfo *info, TypeId self) {
    Expr *field = call->as.call.callee;
    field->as.field.res = (Res){RES_BUILTIN, info->method};
    if (info->self_mode == MODE_MUT) {
        require_mutable(c, field->as.field.base);
    }
    TypeId target = type_argument(c, field, info);
    TypeId params[2];
    uint32_t count = 0;
    while (count < 2 && info->params[count] != BT_NONE) {
        params[count] = param_type(info->params[count], self, target);
        count++;
    }
    TypeId ret = info->ret == BT_NONE ? type_id(TID_UNIT) : param_type(info->ret, self, target);
    Ty sig = {.kind = TY_FN, .inner = ret, .arg_count = count, .args = params};
    return check_args(c, call, &sig);
}

static Receiver receiver_of(const Ty *ty, bool *ok) {
    *ok = ty->kind == TY_STR || ty->kind == TY_INT || ty->kind == TY_FLOAT;
    return ty->kind == TY_STR ? RECV_STR : ty->kind == TY_INT ? RECV_INT : RECV_FLOAT;
}

TypeId check_method_call(Checker *c, Expr *call) {
    Expr *field = call->as.call.callee;
    Expr *base = field->as.field.base;
    Symbol name = field->as.field.name;
    const Def *type_name_def = primitive_name(c, base);
    Receiver receiver = RECV_STR_TYPE;
    TypeId self = type_id(TID_ERROR);
    bool known = true;
    if (type_name_def) {   // an associated function: `str.new()`
        self = type_name_def->type;
        base->type = self;
        known = self.id == TID_STR;
    } else {
        self = check_expr(c, base, NO_TYPE);
        receiver = receiver_of(type_get(c->types, self), &known);
    }
    const BuiltinInfo *info = known ? builtin_method(&c->sema->session->names, receiver, name)
                                    : NULL;
    if (!info) {
        if (self.id != TID_ERROR) {
            check_error(c, "E0301", field->span, "no method `" SV_FMT "` on type `%s`",
                        SV_ARG(name_of(c, name)), type_name(c, self));
        }
        for (uint32_t i = 0; i < call->as.call.args.len; i++) {
            check_expr(c, call->as.call.args.items[i].value, NO_TYPE);
        }
        return type_id(TID_ERROR);
    }
    return call_builtin(c, call, info, self);
}

// `i32.MAX`, `float.NAN`, ...; fields of structs come later.
TypeId check_field(Checker *c, Expr *e) {
    const Def *primitive = primitive_name(c, e->as.field.base);
    if (!primitive) {
        TypeId base = check_expr(c, e->as.field.base, NO_TYPE);
        if (base.id == TID_ERROR) {
            return base;
        }
        return unsupported(c, e->span, "fields are");
    }
    e->as.field.base->type = primitive->type;
    const Ty *ty = type_get(c->types, primitive->type);
    if (builtin_const(&c->sema->session->names, ty, e->as.field.name) == BC_NONE) {
        check_error(c, "E0301", e->span, "no constant `" SV_FMT "` on type `%s`",
                    SV_ARG(name_of(c, e->as.field.name)), type_name(c, primitive->type));
        return type_id(TID_ERROR);
    }
    return primitive->type;
}

// `s[i]` is the byte at offset i of a str (spec 2.3).
TypeId check_index(Checker *c, Expr *e) {
    TypeId base = check_expr(c, e->as.index.base, NO_TYPE);
    coerce(c, e->as.index.index, type_id(TID_INT));
    if (base.id == TID_STR) {
        return type_id(TID_U8);
    }
    if (base.id != TID_ERROR) {
        check_error(c, "E0313", e->span, "type `%s` cannot be indexed", type_name(c, base));
    }
    return type_id(TID_ERROR);
}
