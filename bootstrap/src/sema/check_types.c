// Type expressions → TypeId, function signatures and type compatibility.

#include "sema/check_internal.h"

static TypeId resolve_alias(Checker *c, Def *def, Span span) {
    if (def->checking) {
        check_error(c, "E0310", span, "the type alias `" SV_FMT "` refers to itself",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)));
        return type_id(TID_ERROR);
    }
    if (def->item->as.alias.type_params.len > 0) {
        return unsupported(c, span, "generic type aliases are");
    }
    if (!def->typed) {
        uint32_t saved = sema_enter(c->sema, def->module);
        Generics outer = enter_generics(c, (Generics){0});
        def->checking = true;
        def->type = resolve_type(c, def->item->as.alias.type);
        def->checking = false;
        enter_generics(c, outer);
        def->typed = true;
        sema_enter(c->sema, saved);
    }
    return def->type;
}

Generics enter_generics(Checker *c, Generics generics) {
    Generics saved = c->generics;
    c->generics = generics;
    return saved;
}

TypeId adt_instance(Checker *c, DefId id, const TypeId *args, uint32_t count, Span span) {
    const Def *def = sema_def(c->sema, id);
    uint32_t expected = item_type_params(def->item).len;
    if (count != expected) {
        check_error(c, "E0302", span, "wrong number of type arguments for `" SV_FMT "`: "
                    "expected %u, found %u",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)), expected, count);
        return type_id(TID_ERROR);
    }
    return type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id, .arg_count = count,
                                      .args = args});
}

// Whether a type parameter named `name` is in scope; its type goes to `out`.
static bool type_parameter(Checker *c, Symbol name, TypeId *out) {
    for (uint32_t i = 0; i < c->generics.fn.len; i++) {
        if (symbol_eq(c->generics.fn.items[i], name)) {
            *out = type_param(c->types, c->generics.owner.len + i, name);
            return true;
        }
    }
    for (uint32_t i = 0; i < c->generics.owner.len; i++) {
        if (symbol_eq(c->generics.owner.items[i], name)) {
            *out = type_param(c->types, i, name);
            return true;
        }
    }
    return false;
}

static void resolve_list(Checker *c, TypeList list, TypeId *out);

static TypeId named_type(Checker *c, Def *def, DefId id, const TypeExpr *type) {
    TypeList list = type->as.path.args;
    if (list.len > 0 && def->kind != DEF_STRUCT && def->kind != DEF_ENUM) {
        check_error(c, "E0302", type->span, "`" SV_FMT "` takes no type arguments",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)));
        return type_id(TID_ERROR);
    }
    Span span = type->span;
    switch (def->kind) {
    case DEF_PRIMITIVE: return def->type;
    case DEF_TYPE_ALIAS: return resolve_alias(c, def, span);
    case DEF_STRUCT: case DEF_ENUM: {
        if (list.len == 0) {
            return adt_instance(c, id, NULL, 0, span);
        }
        TypeId *args = ARENA_ARRAY(&c->sema->session->arena, TypeId, list.len);
        resolve_list(c, list, args);
        return adt_instance(c, id, args, list.len, span);
    }
    default:
        check_error(c, "E0310", span, "expected a type, found `" SV_FMT "`",
                    SV_ARG(symbol_name(&c->sema->session->names, def->name)));
        return type_id(TID_ERROR);
    }
}

DefId module_type(Checker *c, uint32_t module, Symbol name, Span span) {
    DefId id = scope_module_in(&c->sema->scope, module, name);
    const Def *def = id.id ? sema_def(c->sema, id) : NULL;
    StrView module_name = c->sema->program.items[module].name;
    if (!def || def->in_prelude || def->module != module) {
        check_error(c, "E0200", span, "cannot find `" SV_FMT "` in module `" SV_FMT "`",
                    SV_ARG(symbol_name(&c->sema->session->names, name)), SV_ARG(module_name));
        return (DefId){0};
    }
    if (!def->is_pub) {
        check_error(c, "E0202", span, "`" SV_FMT "` is private to module `" SV_FMT "`",
                    SV_ARG(symbol_name(&c->sema->session->names, name)), SV_ARG(module_name));
    }
    return id;
}

// `Name` or `module.Name`.
static DefId path_target(Checker *c, const TypeExpr *type, Symbol *name) {
    SymbolList segments = type->as.path.segments;
    *name = segments.items[segments.len - 1];
    DefId id = scope_module(&c->sema->scope, segments.items[0]);
    if (segments.len == 1 || id.id == 0) {
        return segments.len == 1 ? id : (DefId){0};
    }
    const Def *module = sema_def(c->sema, id);
    if (segments.len > 2 || module->kind != DEF_MODULE) {
        unsupported(c, type->span, "nested module paths in types are");
        return (DefId){0};
    }
    return module_type(c, module->target, *name, type->span);
}

static TypeId resolve_path(Checker *c, const TypeExpr *type) {
    TypeId param;
    SymbolList segments = type->as.path.segments;
    if (segments.len == 1 && type_parameter(c, segments.items[0], &param)) {
        if (type->as.path.args.len > 0) {
            check_error(c, "E0302", type->span, "a type parameter takes no type arguments");
        }
        return param;
    }
    Symbol name;
    DefId id = path_target(c, type, &name);
    if (id.id == 0 && type->as.path.segments.len > 1) {
        return type_id(TID_ERROR);   // reported by module_type
    }
    if (id.id == 0) {
        check_error(c, "E0200", type->span, "cannot find type `" SV_FMT "` in this scope",
                    SV_ARG(symbol_name(&c->sema->session->names, name)));
        return type_id(TID_ERROR);
    }
    return named_type(c, sema_def(c->sema, id), id, type);
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
    // A signature may be needed by a caller elsewhere: its names are looked
    // up in its own module, and `Self` is a method's own type.
    uint32_t saved_module = sema_enter(c->sema, def->module);
    TypeId saved_self = c->self_type;
    const Item *owner = def->item;
    Generics outer = enter_generics(c, (Generics){.owner = owner ? item_type_params(owner)
                                                                 : (SymbolList){0},
                                                  .fn = fn->type_params});
    if (owner && owner->kind != ITEM_FN && owner->kind != ITEM_EXTERN) {
        c->self_type = owner_type(c->sema, owner);
    }
    uint32_t count = fn->params.len;
    TypeId *params = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        params[i] = resolve_type(c, fn->params.items[i].type);
    }
    TypeId ret = fn->ret ? resolve_type(c, fn->ret) : type_id(TID_UNIT);
    enter_generics(c, outer);
    c->self_type = saved_self;
    sema_enter(c->sema, saved_module);
    def->type = type_fn(c->types, params, count, ret);
    def->typed = true;
    return def->type;
}
