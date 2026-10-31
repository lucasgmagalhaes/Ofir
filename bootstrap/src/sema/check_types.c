// Type expressions → TypeId, function signatures and type compatibility.

#include "sema/check_internal.h"

static TypeId resolve_alias(Checker *c, Def *def, Span span) {
    if (def->checking) {
        check_error(c, "E0310", span, "the type alias `" SV_FMT "` refers to itself",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)));
        return type_id(TID_ERROR);
    }
    if (!def->typed) {
        def->checking = true;
        def->type = resolve_type(c, def->item->as.alias.type);
        def->checking = false;
        def->typed = true;
    }
    return def->type;
}

static TypeId named_type(Checker *c, Def *def, DefId id, Span span) {
    switch (def->kind) {
    case DEF_PRIMITIVE: return def->type;
    case DEF_TYPE_ALIAS: return resolve_alias(c, def, span);
    case DEF_STRUCT: case DEF_ENUM:
        return type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id});
    default:
        check_error(c, "E0310", span, "expected a type, found `" SV_FMT "`",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)));
        return type_id(TID_ERROR);
    }
}

static TypeId resolve_path(Checker *c, const TypeExpr *type) {
    if (type->as.path.segments.len > 1) {
        return unsupported(c, type->span, "module paths in types are");
    }
    if (type->as.path.args.len > 0) {
        return unsupported(c, type->span, "generic types are");
    }
    Symbol name = type->as.path.segments.items[0];
    DefId id = scope_module(&c->sema->scope, name);
    if (id.id == 0) {
        check_error(c, "E0200", type->span, "cannot find type `" SV_FMT "` in this scope",
                    SV_ARG(symbol_name(&c->sema->session->names, name)));
        return type_id(TID_ERROR);
    }
    return named_type(c, sema_def(c->sema, id), id, type->span);
}

static void resolve_list(Checker *c, TypeList list, TypeId *out) {
    for (uint32_t i = 0; i < list.len; i++) {
        out[i] = resolve_type(c, list.items[i]);
    }
}

static TypeId resolve_array(Checker *c, const TypeExpr *type) {
    TypeId elem = resolve_type(c, type->as.array.elem);
    const Expr *len = type->as.array.len;
    if (!len) {
        return type_slice(c->types, elem);
    }
    if (len->kind != EXPR_INT || len->as.int_value > UINT32_MAX) {
        return unsupported(c, len->span, "array lengths other than integer literals are");
    }
    return type_intern(c->types,
                       (Ty){.kind = TY_ARRAY, .data = (uint32_t)len->as.int_value, .inner = elem});
}

static TypeId resolve_fn_type(Checker *c, const TypeExpr *type) {
    uint32_t count = type->as.fn.params.len;
    TypeId *params = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
    resolve_list(c, type->as.fn.params, params);
    TypeId ret = type->as.fn.ret ? resolve_type(c, type->as.fn.ret) : type_id(TID_UNIT);
    return type_fn(c->types, params, count, ret);
}

// check-sizes: table
TypeId resolve_type(Checker *c, const TypeExpr *type) {
    switch (type->kind) {
    case TYPE_PATH: return resolve_path(c, type);
    case TYPE_SELF:
        if (type_eq(c->self_type, type_id(TID_ERROR))) {
            check_error(c, "E0200", type->span, "`Self` is only available inside a type");
        }
        return c->self_type;
    case TYPE_REF:
        return type_ref(c->types, resolve_type(c, type->as.ref.inner), type->as.ref.mutable);
    case TYPE_PTR: {
        TypeId inner = resolve_type(c, type->as.ref.inner);
        return type_intern(c->types,
                           (Ty){.kind = TY_PTR, .data = type->as.ref.mutable, .inner = inner});
    }
    case TYPE_SLICE: case TYPE_ARRAY: return resolve_array(c, type);
    case TYPE_TUPLE: {
        uint32_t count = type->as.tuple.len;
        TypeId *elems = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
        resolve_list(c, type->as.tuple, elems);
        return type_tuple(c->types, elems, count);
    }
    case TYPE_FN: return resolve_fn_type(c, type);
    case TYPE_NEVER: return type_id(TID_NEVER);
    case TYPE_DYN: return unsupported(c, type->span, "`dyn` types are");
    case TYPE_OPTION: return unsupported(c, type->span, "optional types (`T?`) are");
    }
    return type_id(TID_ERROR);
}

TypeId fn_signature(Checker *c, FnDecl *fn) {
    Def *def = sema_def(c->sema, fn->def);
    if (def->typed) {
        return def->type;
    }
    // A method's signature may be needed by a caller elsewhere: `Self` is
    // the method's own type, whatever is being checked.
    TypeId saved_self = c->self_type;
    const Item *owner = def->item;
    if (owner && owner->kind != ITEM_FN && owner->kind != ITEM_EXTERN) {
        DefId id = scope_module(&c->sema->scope, owner->name);
        c->self_type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id});
    }
    uint32_t count = fn->params.len;
    TypeId *params = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        params[i] = resolve_type(c, fn->params.items[i].type);
    }
    TypeId ret = fn->ret ? resolve_type(c, fn->ret) : type_id(TID_UNIT);
    c->self_type = saved_self;
    def->type = type_fn(c->types, params, count, ret);
    def->typed = true;
    return def->type;
}

// `!` converts to every type; the error type is compatible with everything
// so that one mistake is reported once.
bool types_compatible(TypeId found, TypeId expected) {
    return type_eq(found, expected) || found.id == TID_ERROR || expected.id == TID_ERROR ||
           found.id == TID_NEVER;
}
