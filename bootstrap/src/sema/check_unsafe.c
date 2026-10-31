// `unsafe`, raw pointers and the C interface (spec 11).

#include "sema/check_internal.h"

void require_unsafe(Checker *c, Span span, const char *what) {
    if (c->unsafe_depth == 0) {
        check_error(c, "E0803", span, "%s requires an `unsafe` block or function", what);
    }
}

// `*p`: the value a raw pointer points to.
TypeId check_deref(Checker *c, Expr *e, TypeId expected) {
    TypeId type = check_expr(c, e->as.unary.operand, NO_TYPE);
    const Ty *ty = type_get(c->types, type);
    if (ty->kind == TY_PTR) {
        require_unsafe(c, e->span, "dereferencing a raw pointer");
        return ty->inner;
    }
    (void)expected;
    if (ty->kind == TY_REF) {
        return unsupported(c, e->span, "references are");
    }
    if (type.id != TID_ERROR) {
        check_error(c, "E0313", e->span, "type `%s` cannot be dereferenced", type_name(c, type));
    }
    return type_id(TID_ERROR);
}

// Pointer to pointer, and pointer to or from `uint`/`int` (spec 11.1).
bool pointer_cast(Checker *c, TypeId from, TypeId to, Span span) {
    TyKind a = type_get(c->types, from)->kind, b = type_get(c->types, to)->kind;
    bool ptr_ptr = a == TY_PTR && b == TY_PTR;
    bool ptr_int = (a == TY_PTR && (to.id == TID_U64 || to.id == TID_I64)) ||
                   (b == TY_PTR && (from.id == TID_U64 || from.id == TID_I64));
    if (ptr_ptr || ptr_int) {
        require_unsafe(c, span, "converting a raw pointer");
        return true;
    }
    return false;
}

// Integers, floats, bool, raw pointers and () can cross the C boundary.
static bool c_compatible(Checker *c, TypeId type) {
    TyKind kind = type_get(c->types, type)->kind;
    return kind == TY_INT || kind == TY_FLOAT || kind == TY_BOOL || kind == TY_PTR ||
           kind == TY_UNIT || kind == TY_NEVER || kind == TY_ERROR;
}

void check_c_signature(Checker *c, FnDecl *fn) {
    const Ty *sig = type_get(c->types, fn_signature(c, fn));
    for (uint32_t i = 0; i < sig->arg_count; i++) {
        if (!c_compatible(c, sig->args[i])) {
            check_error(c, "E0804", fn->params.items[i].span,
                        "type `%s` has no C layout and cannot cross the C boundary",
                        type_name(c, sig->args[i]));
        }
    }
    if (!c_compatible(c, sig->inner)) {
        check_error(c, "E0804", fn->span,
                    "type `%s` has no C layout and cannot cross the C boundary",
                    type_name(c, sig->inner));
    }
}

// Whether calling `fn` needs `unsafe`: it is declared in an extern block or
// with `unsafe fn`.
bool is_unsafe_fn(const Def *def) {
    return def->kind == DEF_FN && (def->fn->is_unsafe ||
                                   (def->item && def->item->kind == ITEM_EXTERN));
}
