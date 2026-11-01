// Methods and associated functions declared in a struct's body (spec 3.6),
// and the arguments of `mut` parameters.

#include "sema/check_internal.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

static FnDecl *find_method(Checker *c, DefId owner, Symbol name) {
    const Item *item = sema_def(c->sema, owner)->item;
    FnList methods = item->kind == ITEM_ENUM ? item->as.enum_.methods : item->as.struct_.methods;
    for (uint32_t i = 0; i < methods.len; i++) {
        if (symbol_eq(methods.items[i]->name, name)) {
            return methods.items[i];
        }
    }
    return NULL;
}

bool has_method(Checker *c, DefId owner, Symbol name) {
    return find_method(c, owner, name) != NULL;
}

void check_visible(Checker *c, uint32_t module, bool is_pub, Symbol name, Span span) {
    if (module != c->sema->current && !is_pub) {
        check_error(c, "E0202", span, "`" SV_FMT "` is private to module `" SV_FMT "`",
                    SV_ARG(name_of(c, name)), SV_ARG(c->sema->program.items[module].name));
    }
}

// The struct or enum named by `e`, as in `Point.new()` or `Stack<int>.new()`.
bool type_name_of(Checker *c, const Expr *e, DefId *out, TypeList *args) {
    Res res = e->kind == EXPR_IDENT ? e->as.ident.res
              : e->kind == EXPR_GENERIC ? e->as.generic.res : (Res){0};
    if (res.kind != RES_DEF) {
        return false;
    }
    DefKind kind = sema_def(c->sema, (DefId){res.index})->kind;
    *out = (DefId){res.index};
    *args = e->kind == EXPR_GENERIC ? e->as.generic.args : (TypeList){0};
    return kind == DEF_STRUCT || kind == DEF_ENUM;
}

TypeId named_instance(Checker *c, DefId id, TypeList list, Span span) {
    if (list.len == 0 && item_type_params(sema_def(c->sema, id)->item).len > 0) {
        return type_id(TID_ERROR);
    }
    TypeId *args = ARENA_ARRAY(&c->sema->session->arena, TypeId, list.len + 1);
    for (uint32_t i = 0; i < list.len; i++) {
        args[i] = resolve_type(c, list.items[i]);
    }
    return adt_instance(c, id, args, list.len, span);
}

// An argument for a `mut` parameter must be a place that may be modified;
// the caller passes it by reference (spec 5.3).
void check_mut_args(Checker *c, const FnDecl *fn, Expr *call) {
    ArgList args = call->as.call.args;
    for (uint32_t i = 0; i < fn->params.len && i < args.len; i++) {
        if (fn->params.items[i].mode == MODE_MUT) {
            require_mutable(c, args.items[i].value);
        }
    }
}

static TypeId skip_args(Checker *c, Expr *call) {
    for (uint32_t i = 0; i < call->as.call.args.len; i++) {
        check_expr(c, call->as.call.args.items[i].value, NO_TYPE);
    }
    return type_id(TID_ERROR);
}

static TypeId wrong_call(Checker *c, Expr *call, const char *message, Symbol name) {
    check_error(c, "E0301", call->as.call.callee->span, message, SV_ARG(name_of(c, name)));
    return skip_args(c, call);
}

static TypeId method_args(Checker *c, Expr *call, FnDecl *fn, TypeHints hints) {
    Expr *field = call->as.call.callee;
    const Def *def = sema_def(c->sema, fn->def);
    const Ty *sig = type_get(c->types, fn_signature(c, fn));
    TypeList explicit = field->as.field.type_args;
    if (fn_type_param_count(def) == 0) {
        if (explicit.len > 0) {
            check_error(c, "E0302", field->span, "`" SV_FMT "` takes no type arguments",
                        SV_ARG(name_of(c, fn->name)));
        }
        return check_args(c, call, sig);
    }
    Infer inf = infer_new(c, fn_generics(def));
    infer_from_self(&inf, type_get(c->types, hints.self), inf.params.owner.len);
    if (explicit.len > 0) {
        infer_explicit(c, &inf, false, explicit, field->span);
    }
    return check_generic_args(c, call, sig, &inf, hints.expected);
}

// `value.m(args)` (via_type false) or `Type.f(args)` (via_type true).
TypeId check_user_method(Checker *c, Expr *call, DefId owner, bool via_type, TypeHints hints) {
    Expr *field = call->as.call.callee;
    Symbol name = field->as.field.name;
    FnDecl *fn = find_method(c, owner, name);
    if (!fn) {
        TypeId type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = owner.id});
        check_error(c, "E0301", field->span, "no method `" SV_FMT "` on type `%s`",
                    SV_ARG(name_of(c, name)), type_name(c, type));
        return skip_args(c, call);
    }
    if (via_type && fn->has_self) {
        return wrong_call(c, call, "`" SV_FMT "` is a method: call it on a value, `value.m()`",
                          name);
    }
    if (!via_type && !fn->has_self) {
        return wrong_call(c, call,
                          "`" SV_FMT "` is an associated function: call it on the type, "
                          "`Type.f()`", name);
    }
    if (fn->has_self && fn->self_mode == MODE_MUT) {
        require_mutable(c, field->as.field.base);
    }
    check_visible(c, sema_def(c->sema, owner)->module, fn->is_pub, name, field->span);
    field->as.field.res = (Res){RES_DEF, fn->def.id};
    TypeId ret = method_args(c, call, fn, hints);
    if (owner.id == c->sema->list.id && sv_eq(name_of(c, name), SV("sort"))) {
        check_list_sort(c, call);
    }
    check_mut_args(c, fn, call);
    return ret;
}
