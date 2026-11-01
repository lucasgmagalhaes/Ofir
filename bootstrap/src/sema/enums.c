// Enum variants and their construction (spec 2.5 and 3.5): `.Circle(1.0)`
// where the enum is known from the context, `Shape.Circle(1.0)` and
// `Shape.Empty`.

#include "sema/check_internal.h"
#include "sema/const_eval.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

static void fill_variant(Checker *c, const Variant *decl, VariantInfo *v, int64_t *next) {
    Arena *arena = &c->sema->session->arena;
    v->name = decl->name;
    v->count = decl->fields.len;
    v->field_names = ARENA_ARRAY(arena, Symbol, v->count + 1);
    v->types = ARENA_ARRAY(arena, TypeId, v->count + 1);
    for (uint32_t f = 0; f < v->count; f++) {
        v->field_names[f] = decl->fields.items[f].name;
        v->types[f] = resolve_type(c, decl->fields.items[f].type);
        check_not_recursive(c, v->types[f], decl->span);
    }
    v->discriminant = *next;
    if (decl->discriminant) {
        TypeId type = coerce(c, decl->discriminant, type_id(TID_I64));
        const ConstValue *value = type.id == TID_ERROR ? NULL : const_eval(c, decl->discriminant);
        if (value) {
            v->discriminant = (int64_t)value->bits;
        }
    }
    *next = v->discriminant + 1;
}

// Explicit discriminants: only without payloads, and all distinct.
static void check_discriminants(Checker *c, const Item *item, const EnumInfo *info) {
    for (uint32_t i = 0; i < info->count; i++) {
        const Variant *decl = &item->as.enum_.variants.items[i];
        if (decl->discriminant && info->has_payload) {
            check_error(c, "E0319", decl->span,
                        "explicit discriminants are allowed only in enums without payloads");
            return;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (info->variants[j].discriminant == info->variants[i].discriminant) {
                check_error(c, "E0319", decl->span, "discriminant %lld is used twice",
                            (long long)info->variants[i].discriminant);
            }
        }
    }
}

EnumInfo *enum_variants(Checker *c, DefId id) {
    Def *def = sema_def(c->sema, id);
    if (def->variants || def->checking) {
        return def->variants;
    }
    const Item *item = def->item;
    EnumInfo *info = ARENA_NEW(&c->sema->session->arena, EnumInfo);
    info->count = item->as.enum_.variants.len;
    info->variants = ARENA_ARRAY(&c->sema->session->arena, VariantInfo, info->count + 1);
    uint32_t saved_module = sema_enter(c->sema, def->module);
    TypeId saved_self = c->self_type;
    Generics outer = enter_generics(c, (Generics){.owner = item->as.enum_.type_params});
    c->self_type = owner_type(c->sema, item);
    def->checking = true;
    int64_t next = 0;
    for (uint32_t i = 0; i < info->count; i++) {
        fill_variant(c, &item->as.enum_.variants.items[i], &info->variants[i], &next);
        info->has_payload = info->has_payload || info->variants[i].count > 0;
    }
    def->checking = false;
    enter_generics(c, outer);
    c->self_type = saved_self;
    sema_enter(c->sema, saved_module);
    def->variants = info;
    check_discriminants(c, item, info);
    return info;
}

int64_t variant_index(const EnumInfo *info, Symbol name) {
    for (uint32_t i = 0; i < info->count; i++) {
        if (symbol_eq(info->variants[i].name, name)) {
            return i;
        }
    }
    return -1;
}

// The enum of type `type`, or NULL.
EnumInfo *enum_of(Checker *c, TypeId type, DefId *id) {
    const Ty *ty = type_get(c->types, type);
    if (ty->kind != TY_ADT || sema_def(c->sema, (DefId){ty->data})->kind != DEF_ENUM) {
        return NULL;
    }
    *id = (DefId){ty->data};
    return enum_variants(c, *id);
}

// A variant being built: `.Circle(1.0)`, `Shape.Circle(1.0)` or `.Empty`.
typedef struct {
    Expr *at;
    const ArgList *args;   // NULL without parentheses
    DefId id;
    int64_t index;
} VariantUse;

// The payload of a variant of a generic enum: its type arguments come from
// `hints.self` (`Opt<int>.None`), the expected type or the payload.
static TypeId build_generic(Checker *c, VariantUse use, const VariantInfo *v, TypeHints hints) {
    const Item *item = sema_def(c->sema, use.id)->item;
    Infer inf = infer_new(c, (Generics){.owner = item->as.enum_.type_params});
    infer_from_self(&inf, type_get(c->types, hints.self), inf.count);
    Ty sig = {.kind = TY_FN, .inner = owner_type(c->sema, item), .arg_count = v->count,
              .args = v->types};
    Expr call = {.kind = EXPR_CALL, .span = use.at->span};
    if (use.args && v->count > 0) {
        call.as.call.args = *use.args;
    } else {
        sig.arg_count = 0;
    }
    return check_generic_args(c, &call, &sig, &inf, hints.expected);
}

// Checks the payload of a variant.
static TypeId build(Checker *c, VariantUse use, TypeHints hints) {
    const EnumInfo *info = enum_variants(c, use.id);
    TypeId type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = use.id.id});
    const VariantInfo *v = &info->variants[use.index];
    if (!use.args && v->count > 0) {
        check_error(c, "E0302", use.at->span, "the variant `" SV_FMT "` takes %u values",
                    SV_ARG(name_of(c, v->name)), v->count);
    }
    if (use.args && v->count == 0) {
        check_error(c, "E0302", use.at->span, "the variant `" SV_FMT "` takes no values: "
                    "write it without parentheses", SV_ARG(name_of(c, v->name)));
    }
    if (sema_def(c->sema, use.id)->item->as.enum_.type_params.len > 0) {
        return build_generic(c, use, v, hints);
    }
    if (!use.args || v->count == 0) {
        return type;
    }
    Ty sig = {.kind = TY_FN, .inner = type, .arg_count = v->count, .args = v->types};
    Expr call = {.kind = EXPR_CALL, .span = use.at->span};
    call.as.call.args = *use.args;
    return check_args(c, &call, &sig);
}

static TypeId no_variant(Checker *c, Span span, Symbol name, TypeId type) {
    check_error(c, "E0320", span, "no variant `" SV_FMT "` in `%s`", SV_ARG(name_of(c, name)),
                type_name(c, type));
    return type_id(TID_ERROR);
}

// `.Circle(1.0)`: the enum comes from the expected type.
TypeId check_variant_shorthand(Checker *c, Expr *e, TypeId expected) {
    DefId id;
    EnumInfo *info = enum_of(c, expected, &id);
    if (!info) {
        check_error(c, "E0320", e->span, "cannot tell which enum `." SV_FMT "` belongs to: "
                    "write `Enum." SV_FMT "`", SV_ARG(name_of(c, e->as.variant.name)),
                    SV_ARG(name_of(c, e->as.variant.name)));
        return type_id(TID_ERROR);
    }
    int64_t index = variant_index(info, e->as.variant.name);
    if (index < 0) {
        return no_variant(c, e->span, e->as.variant.name, expected);
    }
    e->as.variant.index = (uint32_t)index;
    VariantUse use = {e, e->as.variant.has_args ? &e->as.variant.args : NULL, id, index};
    return build(c, use, (TypeHints){expected, expected});
}

// `Shape.Circle(1.0)` (call) or `Shape.Empty` (field); false if `name` is
// not a variant, so that the caller looks for a method.
bool check_qualified_variant(Checker *c, Expr *e, DefId id, TypeHints hints, TypeId *out) {
    Expr *field = e->kind == EXPR_CALL ? e->as.call.callee : e;
    Symbol name = field->as.field.name;
    EnumInfo *info = enum_variants(c, id);
    int64_t index = info ? variant_index(info, name) : -1;
    if (index < 0) {
        return false;
    }
    field->as.field.res = (Res){RES_VARIANT, (uint32_t)index};
    VariantUse use = {e, e->kind == EXPR_CALL ? &e->as.call.args : NULL, id, index};
    *out = build(c, use, hints);
    return true;
}
