// Struct fields, literals, tuple struct constructors and field access
// (spec 3.4).

#include "sema/check_internal.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

static bool is_copy_type(Checker *c, TypeId type) {
    const Ty *ty = type_get(c->types, type);
    if (ty->kind == TY_ADT) {
        const Def *def = sema_def(c->sema, (DefId){ty->data});
        return def->kind == DEF_STRUCT && def->item->as.struct_.is_copy;
    }
    return ty->kind != TY_STR;   // the primitive types except str
}

// A field of a struct or enum type that is still being laid out contains
// itself. Tuples are looked into.
void check_not_recursive(Checker *c, TypeId type, Span span) {
    const Ty *ty = type_get(c->types, type);
    for (uint32_t i = 0; ty->kind == TY_TUPLE && i < ty->arg_count; i++) {
        check_not_recursive(c, ty->args[i], span);
    }
    if (ty->kind != TY_ADT) {
        return;
    }
    Def *inner = sema_def(c->sema, (DefId){ty->data});
    if (inner->checking) {
        check_error(c, "E0318", span,
                    "a type cannot contain itself by value; use Box<T> or a collection");
    } else if (inner->kind == DEF_STRUCT) {
        struct_fields(c, (DefId){ty->data});
    } else if (inner->kind == DEF_ENUM) {
        enum_variants(c, (DefId){ty->data});
    }
}

static void fill_fields(Checker *c, Def *def, StructInfo *info) {
    const Item *item = def->item;
    for (uint32_t i = 0; i < info->count; i++) {
        const FieldDecl *field = &item->as.struct_.fields.items[i];
        info->names[i] = field->name;
        info->types[i] = resolve_type(c, field->type);
        info->defaults[i] = field->default_value;
        check_not_recursive(c, info->types[i], field->span);
        if (field->default_value) {
            coerce(c, field->default_value, info->types[i]);
        }
        if (info->is_copy && !is_copy_type(c, info->types[i]) && info->types[i].id != TID_ERROR) {
            check_error(c, "E0317", field->span,
                        "a `copy struct` can only have fields of Copy types, not `%s`",
                        type_name(c, info->types[i]));
        }
    }
}

StructInfo *struct_fields(Checker *c, DefId id) {
    Def *def = sema_def(c->sema, id);
    if (def->fields || def->checking) {
        return def->fields;
    }
    const Item *item = def->item;
    if (item->as.struct_.type_params.len > 0) {
        return NULL;   // generic structs come with roadmap 2.18
    }
    Arena *arena = &c->sema->session->arena;
    StructInfo *info = ARENA_NEW(arena, StructInfo);
    info->count = item->as.struct_.fields.len;
    info->names = ARENA_ARRAY(arena, Symbol, info->count + 1);
    info->types = ARENA_ARRAY(arena, TypeId, info->count + 1);
    info->defaults = ARENA_ARRAY(arena, Expr *, info->count + 1);
    info->is_tuple = item->kind == ITEM_TUPLE_STRUCT;
    info->is_copy = item->as.struct_.is_copy;
    uint32_t saved_module = sema_enter(c->sema, def->module);
    TypeId saved_self = c->self_type;
    c->self_type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id});
    def->checking = true;
    fill_fields(c, def, info);
    def->checking = false;
    c->self_type = saved_self;
    sema_enter(c->sema, saved_module);
    def->fields = info;
    return info;
}

// The struct definition of a type, or NULL.
static const Def *struct_def(Checker *c, TypeId type, DefId *id) {
    const Ty *ty = type_get(c->types, type);
    if (ty->kind != TY_ADT) {
        return NULL;
    }
    *id = (DefId){ty->data};
    const Def *def = sema_def(c->sema, *id);
    return def->kind == DEF_STRUCT ? def : NULL;
}

static int64_t field_index(const StructInfo *info, Symbol name) {
    for (uint32_t i = 0; i < info->count; i++) {
        if (symbol_eq(info->names[i], name)) {
            return i;
        }
    }
    return -1;
}

TypeId check_struct_field(Checker *c, Expr *e, TypeId base) {
    DefId id;
    const Def *def = struct_def(c, base, &id);
    StructInfo *info = def ? struct_fields(c, id) : NULL;
    int64_t index = info && !info->is_tuple ? field_index(info, e->as.field.name) : -1;
    if (index < 0) {
        if (base.id != TID_ERROR && (!def || info)) {
            check_error(c, "E0316", e->span, "no field `" SV_FMT "` on type `%s`",
                        SV_ARG(name_of(c, e->as.field.name)), type_name(c, base));
        }
        return type_id(TID_ERROR);
    }
    e->as.field.res = (Res){RES_FIELD, (uint32_t)index};
    check_visible(c, def->module, def->item->as.struct_.fields.items[index].is_pub,
                  e->as.field.name, e->span);
    return info->types[index];
}

TypeId check_tuple_field(Checker *c, Expr *e) {
    TypeId base = check_expr(c, e->as.tuple_field.base, NO_TYPE);
    const Ty *ty = type_get(c->types, base);
    if (ty->kind == TY_TUPLE && e->as.tuple_field.index < ty->arg_count) {
        return ty->args[e->as.tuple_field.index];
    }
    DefId id;
    const Def *def = struct_def(c, base, &id);
    StructInfo *info = def ? struct_fields(c, id) : NULL;
    uint32_t index = e->as.tuple_field.index;
    if (!info || !info->is_tuple || index >= info->count) {
        if (base.id != TID_ERROR) {
            check_error(c, "E0316", e->span, "no field `%u` on type `%s`", index,
                        type_name(c, base));
        }
        return type_id(TID_ERROR);
    }
    return info->types[index];
}

// `Name(a, b)` for a tuple struct.
TypeId check_constructor(Checker *c, Expr *call, DefId id) {
    StructInfo *info = struct_fields(c, id);
    TypeId type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id});
    if (!info || !info->is_tuple) {
        check_error(c, "E0310", call->span, "expected a value, found type `" SV_FMT "`; "
                    "build a struct with `Name{field: value}`",
                    SV_ARG(name_of(c, sema_def(c->sema, id)->name)));
        return type_id(TID_ERROR);
    }
    Ty sig = {.kind = TY_FN, .inner = type, .arg_count = info->count, .args = info->types};
    return check_args(c, call, &sig);
}

static void check_literal_fields(Checker *c, Expr *e, const Def *def, const StructInfo *info,
                                 bool *given) {
    for (uint32_t i = 0; i < e->as.struct_.fields.len; i++) {
        FieldInit *init = &e->as.struct_.fields.items[i];
        int64_t index = field_index(info, init->name);
        if (index < 0) {
            check_error(c, "E0316", init->value->span, "no field `" SV_FMT "` on this struct",
                        SV_ARG(name_of(c, init->name)));
            check_expr(c, init->value, NO_TYPE);
            continue;
        }
        check_visible(c, def->module, def->item->as.struct_.fields.items[index].is_pub,
                      init->name, init->value->span);
        if (given[index]) {
            check_error(c, "E0201", init->value->span, "field `" SV_FMT "` is given twice",
                        SV_ARG(name_of(c, init->name)));
        }
        given[index] = true;
        coerce(c, init->value, info->types[index]);
    }
}

// `Point{x: 1, y}`, `Config{}`, `User{age: 31, ..u}`.
TypeId check_struct_literal(Checker *c, Expr *e) {
    Expr *name = e->as.struct_.type;
    if (name->kind != EXPR_IDENT || name->as.ident.res.kind != RES_DEF) {
        return unsupported(c, name->span, "struct literals of generic types are");
    }
    DefId id = {name->as.ident.res.index};
    const Def *def = sema_def(c->sema, id);
    StructInfo *info = def->kind == DEF_STRUCT ? struct_fields(c, id) : NULL;
    if (!info || info->is_tuple) {
        check_error(c, "E0310", name->span, "`" SV_FMT "` is not a struct with named fields",
                    SV_ARG(name_of(c, def->name)));
        return type_id(TID_ERROR);
    }
    TypeId type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = id.id});
    name->type = type;
    bool *given = ARENA_ARRAY(&c->sema->session->arena, bool, info->count + 1);
    check_literal_fields(c, e, def, info, given);
    if (e->as.struct_.base) {
        coerce(c, e->as.struct_.base, type);
        return type;
    }
    for (uint32_t i = 0; i < info->count; i++) {
        if (!given[i] && !info->defaults[i]) {
            check_error(c, "E0308", e->span, "missing field `" SV_FMT "` in `" SV_FMT "`",
                        SV_ARG(name_of(c, info->names[i])), SV_ARG(name_of(c, def->name)));
        }
    }
    return type;
}
