// Bounds on type parameters (spec 8.1), as far as stage 0 goes: `Eq`, `Ord`
// and `Hash`, which a generic body uses through `==`, `<` and `hashOf`.
// Stage 0 compares and hashes the types it can without derived code
// (scalars, `str`, pointers, enums without payloads, tuples of these); a
// struct or enum that stage 1 derives the interface for is a stage 0 limit.

#include "sema/check_internal.h"
#include "syntax/ast_text.h"

static uint32_t bound_bit(Checker *c, const TypeExpr *t) {
    if (t->kind != TYPE_PATH || t->as.path.segments.len != 1 || t->as.path.args.len) {
        return 0;
    }
    StrView name = symbol_name(&c->sema->session->names, t->as.path.segments.items[0]);
    return sv_eq(name, SV("Eq")) ? BOUND_EQ
         : sv_eq(name, SV("Ord")) ? BOUND_ORD
         : sv_eq(name, SV("Hash")) ? BOUND_HASH : 0;
}

static bool names_param(const TypeExpr *t, Symbol name) {
    return t->kind == TYPE_PATH && t->as.path.segments.len == 1 && t->as.path.args.len == 0 &&
           symbol_eq(t->as.path.segments.items[0], name);
}

static uint32_t bits_of(Checker *c, WhereList list, Symbol name) {
    uint32_t bits = 0;
    for (uint32_t i = 0; i < list.len; i++) {
        if (names_param(list.items[i].type, name)) {
            for (uint32_t j = 0; j < list.items[i].bounds.len; j++) {
                bits |= bound_bit(c, list.items[i].bounds.items[j]);
            }
        }
    }
    return bits;
}

uint32_t param_bounds(Checker *c, TypeId type) {
    const Ty *ty = type_get(c->types, type);
    if (ty->kind != TY_PARAM) {
        return 0;
    }
    Symbol name = {ty->inner.id};
    return bits_of(c, c->generics.owner_bounds, name) | bits_of(c, c->generics.fn_bounds, name);
}

static bool all_satisfy(Checker *c, TypeId type, uint32_t bit) {
    const Ty *ty = type_get(c->types, type);
    uint32_t count = ty->arg_count;
    const TypeId *args = ty->args;
    for (uint32_t i = 0; i < count; i++) {
        if (!satisfies(c, args[i], bit)) {
            return false;
        }
    }
    return true;
}

bool satisfies(Checker *c, TypeId type, uint32_t bit) {
    const Ty *ty = type_get(c->types, type);
    TyKind kind = ty->kind;
    if (kind == TY_ERROR || kind == TY_NEVER) {
        return true;
    }
    if (kind == TY_PARAM) {
        return (param_bounds(c, type) & bit) != 0;
    }
    if (bit == BOUND_ORD) {
        return kind == TY_INT || kind == TY_FLOAT || kind == TY_STR;
    }
    if (kind == TY_TUPLE) {
        return all_satisfy(c, type, bit);
    }
    if (bit == BOUND_HASH) {
        return kind == TY_INT || kind == TY_BOOL || kind == TY_STR;
    }
    DefId id;
    const EnumInfo *info = kind == TY_ADT ? enum_of(c, type, &id) : NULL;
    return kind <= TY_STR || kind == TY_PTR || (info && !info->has_payload);
}

// A struct or enum without `drop`, or a tuple: stage 1 may derive the
// interface (spec 9.9) where stage 0 cannot.
static bool derivable(Checker *c, TypeId type) {
    const Ty *ty = type_get(c->types, type);
    if (ty->kind == TY_TUPLE) {
        return true;
    }
    const Def *def = ty->kind == TY_ADT ? sema_def(c->sema, (DefId){ty->data}) : NULL;
    if (!def || !def->item) {
        return false;
    }
    const Item *item = def->item;
    FnList methods = item->kind == ITEM_ENUM ? item->as.enum_.methods : item->as.struct_.methods;
    for (uint32_t i = 0; i < methods.len; i++) {
        if (sv_eq(symbol_name(&c->sema->session->names, methods.items[i]->name), SV("drop"))) {
            return false;
        }
    }
    return true;
}

static const char *bound_name(uint32_t bit) {
    return bit == BOUND_EQ ? "Eq" : bit == BOUND_ORD ? "Ord" : "Hash";
}

void check_bounds(Checker *c, Generics params, const TypeId *args, uint32_t count, Span span) {
    uint32_t owner = params.owner.len;
    for (uint32_t i = 0; i < count && i < owner + params.fn.len; i++) {
        Symbol name = i < owner ? params.owner.items[i] : params.fn.items[i - owner];
        uint32_t bits = bits_of(c, params.owner_bounds, name) | bits_of(c, params.fn_bounds, name);
        for (uint32_t bit = 1; bit <= BOUND_HASH; bit <<= 1) {
            if (!(bits & bit) || satisfies(c, args[i], bit)) {
                continue;
            }
            StrView param = symbol_name(&c->sema->session->names, name);
            if (derivable(c, args[i])) {
                check_error(c, "E0901", span, "the bound `%s` of `" SV_FMT "` on `%s` is not "
                            "supported by stage 0", bound_name(bit), SV_ARG(param),
                            type_name(c, args[i]));
            } else {
                check_error(c, "E0332", span, "the type `%s` does not satisfy the bound `%s` "
                            "of `" SV_FMT "`", type_name(c, args[i]), bound_name(bit),
                            SV_ARG(param));
            }
        }
    }
}

static bool names_any(const TypeExpr *t, SymbolList params) {
    for (uint32_t i = 0; i < params.len; i++) {
        if (names_param(t, params.items[i])) {
            return true;
        }
    }
    return false;
}

void check_bound_names(Checker *c, WhereList bounds) {
    for (uint32_t i = 0; i < bounds.len; i++) {
        const TypeExpr *bounded = bounds.items[i].type;
        if (!names_any(bounded, c->generics.owner) && !names_any(bounded, c->generics.fn)) {
            StrBuf text = {0};
            type_text(&text, bounded, &c->sema->session->names);
            check_error(c, "E0334", bounded->span, "`%s` is not a type parameter of this "
                        "declaration: only type parameters take bounds",
                        text.data ? text.data : "");
            strbuf_free(&text);
        }
        for (uint32_t j = 0; j < bounds.items[i].bounds.len; j++) {
            const TypeExpr *t = bounds.items[i].bounds.items[j];
            if (bound_bit(c, t) == 0) {
                StrBuf text = {0};
                type_text(&text, t, &c->sema->session->names);
                check_error(c, "E0901", t->span, "the bound `%s` is not supported by stage 0",
                            text.data ? text.data : "");
                strbuf_free(&text);
            }
        }
    }
}
