// Type checking of patterns (spec appendix A.7), for `let`, `match` and
// `if let`.

#include "sema/check_internal.h"
#include "sema/suggest.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

static void mismatch(Checker *c, Span span, const char *what, TypeId type) {
    if (type.id != TID_ERROR) {
        check_error(c, "E0300", span, "mismatched types: %s pattern for a value of type %s",
                    what, type_name(c, type));
    }
}

static void check_list(Checker *c, PatternList list, const TypeId *types, uint32_t count,
                       Span span) {
    if (list.len != count) {
        check_error(c, "E0302", span, "wrong number of fields in the pattern: expected %u, "
                    "found %u", count, list.len);
    }
    for (uint32_t i = 0; i < list.len; i++) {
        check_pattern(c, list.items[i], i < count ? types[i] : type_id(TID_ERROR));
    }
}

// The definition named by the single-segment path of a pattern.
static bool path_def(Checker *c, SymbolList path, Span span, DefId *out) {
    if (path.len == 2) {   // `module.Type`
        DefId module = scope_module(&c->sema->scope, path.items[0]);
        note_import_use(c->sema, path.items[0]);
        if (module.id && sema_def(c->sema, module)->kind == DEF_MODULE) {
            *out = module_type(c, sema_def(c->sema, module)->target, path.items[1], span);
            return out->id != 0;
        }
    }
    if (path.len != 1) {
        unsupported(c, span, "nested module paths in patterns are");
        return false;
    }
    *out = scope_module(&c->sema->scope, path.items[0]);
    note_import_use(c->sema, path.items[0]);
    if (out->id == 0) {
        check_error(c, "E0200", span, "cannot find `" SV_FMT "` in this scope",
                    SV_ARG(name_of(c, path.items[0])));
        suggest_name(c->sema, path.items[0], SUGGEST_ITEM);
        return false;
    }
    return true;
}

static bool same_adt(TypeId type, const Ty *ty, DefId id) {
    return type.id != TID_ERROR && ty->kind == TY_ADT && ty->data == id.id;
}

// The field types of a struct, or of variant `v` of an enum (v < 0 for a
// struct), in the instance `type`.
static const TypeId *field_types(Checker *c, TypeId type, int64_t v, uint32_t count) {
    TypeId *types = ARENA_ARRAY(&c->sema->session->arena, TypeId, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        types[i] = v < 0 ? struct_field_type(c->sema, type, i)
                         : variant_field_type(c->sema, type, (uint32_t)v, i);
    }
    return types;
}

static void variant_pattern(Checker *c, Pattern *p, TypeId type) {
    DefId id;
    EnumInfo *info = enum_of(c, type, &id);
    DefId named;
    if (p->as.variant.path.len > 0 && path_def(c, p->as.variant.path, p->span, &named) &&
        info && named.id != id.id) {
        check_error(c, "E0300", p->span, "mismatched types: the pattern is a variant of `"
                    SV_FMT "`, the value has type %s",
                    SV_ARG(name_of(c, sema_def(c->sema, named)->name)), type_name(c, type));
        return;
    }
    if (!info) {
        mismatch(c, p->span, "an enum variant", type);
        return;
    }
    int64_t index = variant_index(info, p->as.variant.name);
    if (index < 0) {
        check_error(c, "E0320", p->span, "no variant `" SV_FMT "` in `%s`",
                    SV_ARG(name_of(c, p->as.variant.name)), type_name(c, type));
        return;
    }
    p->as.variant.index = (uint32_t)index;
    if (p->as.variant.path.len == 0) {   // `.Some(x)` or a bare `Some(x)`
        unqualified_variant(c, p->span, id, p->as.variant.name);
    }
    const VariantInfo *v = &info->variants[index];
    if (p->as.variant.has_args || v->count > 0) {
        check_list(c, p->as.variant.args, field_types(c, type, index, v->count), v->count,
                   p->span);
    }
}

static void struct_pattern(Checker *c, Pattern *p, TypeId type) {
    DefId id;
    if (!path_def(c, p->as.struct_.path, p->span, &id)) {
        return;
    }
    const Ty *ty = type_get(c->types, type);
    StructInfo *info = sema_def(c->sema, id)->kind == DEF_STRUCT ? struct_fields(c, id) : NULL;
    if (!info || info->is_tuple || !same_adt(type, ty, id)) {
        mismatch(c, p->span, "a struct", type);
        return;
    }
    uint32_t listed = 0;
    for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
        FieldPattern *f = &p->as.struct_.fields.items[i];
        uint32_t index = 0;
        while (index < info->count && !symbol_eq(info->names[index], f->name)) {
            index++;
        }
        if (index == info->count) {
            check_error(c, "E0316", p->span, "no field `" SV_FMT "` on type `%s`",
                        SV_ARG(name_of(c, f->name)), type_name(c, type));
            continue;
        }
        listed++;
        check_pattern(c, f->pattern, struct_field_type(c->sema, type, index));
    }
    if (!p->as.struct_.rest && listed < info->count) {
        check_error(c, "E0308", p->span, "the pattern does not list every field: add `..` to "
                    "ignore the others");
    }
}

static void tuple_struct_pattern(Checker *c, Pattern *p, TypeId type) {
    DefId id;
    if (!path_def(c, p->as.tuple_struct.path, p->span, &id)) {
        return;
    }
    if (sema_def(c->sema, id)->kind == DEF_ENUM) {   // `Shape(...)` is not a variant
        mismatch(c, p->span, "a tuple struct", type);
        return;
    }
    StructInfo *info = sema_def(c->sema, id)->kind == DEF_STRUCT ? struct_fields(c, id) : NULL;
    if (!info || !info->is_tuple || !same_adt(type, type_get(c->types, type), id)) {
        mismatch(c, p->span, "a tuple struct", type);
        return;
    }
    check_list(c, p->as.tuple_struct.args, field_types(c, type, -1, info->count), info->count,
               p->span);
}

static void literal_pattern(Checker *c, Pattern *p, TypeId type) {
    TyKind kind = type_get(c->types, type)->kind;
    if (kind != TY_INT && kind != TY_BOOL && kind != TY_STR && kind != TY_ERROR) {
        mismatch(c, p->span, "a literal", type);
        return;
    }
    coerce(c, p->as.literal, type);
}

static void range_pattern(Checker *c, Pattern *p, TypeId type) {
    if (!type_is_integer(c->types, type)) {
        mismatch(c, p->span, "a range", type);
        return;
    }
    coerce(c, p->as.range.lo, type);
    coerce(c, p->as.range.hi, type);
}

void check_pattern(Checker *c, Pattern *p, TypeId type) {
    const Ty *ty = type_get(c->types, type);
    p->type = type;
    switch (p->kind) {
    case PAT_WILDCARD: break;
    case PAT_BINDING:
        c->local_types[p->as.binding.local.id] = type;
        note_owned(c, type);
        break;
    case PAT_LITERAL: literal_pattern(c, p, type); break;
    case PAT_RANGE: range_pattern(c, p, type); break;
    case PAT_VARIANT: variant_pattern(c, p, type); break;
    case PAT_STRUCT: struct_pattern(c, p, type); break;
    case PAT_TUPLE_STRUCT: tuple_struct_pattern(c, p, type); break;
    case PAT_TUPLE:
        if (ty->kind == TY_TUPLE || (type.id == TID_UNIT && p->as.tuple.len == 0)) {   // `()`
            check_list(c, p->as.tuple, ty->args, ty->arg_count, p->span);
        } else {
            mismatch(c, p->span, "a tuple", type);
        }
        break;
    case PAT_OR:
        for (uint32_t i = 0; i < p->as.alternatives.len; i++) {
            check_pattern(c, p->as.alternatives.items[i], type);
        }
        break;
    }
}

static bool all_irrefutable(Checker *c, PatternList list) {
    for (uint32_t i = 0; i < list.len; i++) {
        if (!pattern_irrefutable(c, list.items[i])) {
            return false;
        }
    }
    return true;
}

bool pattern_irrefutable(Checker *c, const Pattern *p) {
    switch (p->kind) {
    case PAT_WILDCARD: case PAT_BINDING: return true;
    case PAT_TUPLE: return all_irrefutable(c, p->as.tuple);
    case PAT_TUPLE_STRUCT: return all_irrefutable(c, p->as.tuple_struct.args);
    case PAT_STRUCT:
        for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
            if (!pattern_irrefutable(c, p->as.struct_.fields.items[i].pattern)) {
                return false;
            }
        }
        return true;
    default: return false;
    }
}
