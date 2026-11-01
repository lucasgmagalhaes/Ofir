// Dereferencing boxes and references. `Box<T>` (docs/std/collections.md):
// `*b` reads the value, and the fields and methods of the value are reached
// through the box; the checker rewrites these uses into a dereference of
// the box's pointer, `*(b.ptr)`, which the lowering handles like any raw
// pointer. Inside the module that declares `Box`, its own fields are
// reached as usual. A reference `&T` is a pointer: `r.x` is `(*r).x`.

#include "sema/check_internal.h"

bool box_inner(Checker *c, TypeId type, TypeId *inner) {
    const Ty *ty = type_get(c->types, type);
    if (c->sema->box.id == 0 || ty->kind != TY_ADT || ty->data != c->sema->box.id) {
        return false;
    }
    *inner = ty->args[0];
    return true;
}

TypeId box_deref(Checker *c, Expr *deref, TypeId box) {
    Expr *operand = deref->as.unary.operand;
    const StructInfo *info = struct_fields(c, c->sema->box);
    Expr *ptr = ARENA_NEW(&c->sema->session->arena, Expr);
    ptr->kind = EXPR_FIELD;
    ptr->span = operand->span;
    ptr->type = struct_field_type(c->sema, box, 0);
    ptr->as.field.base = operand;
    ptr->as.field.name = info->names[0];
    ptr->as.field.res = (Res){RES_FIELD, 0};
    deref->as.unary.operand = ptr;
    return type_get(c->types, ptr->type)->inner;
}

static bool own_box(Checker *c) {
    return c->sema->box.id && sema_def(c->sema, c->sema->box)->module == c->sema->current;
}

TypeId deref_once(Checker *c, Expr **place, TypeId type) {
    TypeId inner;
    const Ty *ty = type_get(c->types, type);
    bool box = !own_box(c) && box_inner(c, type, &inner);
    if (!box && ty->kind != TY_REF) {
        return type;
    }
    Expr *deref = ARENA_NEW(&c->sema->session->arena, Expr);
    deref->kind = EXPR_UNARY;
    deref->span = (*place)->span;
    deref->as.unary.op = OP_DEREF;
    deref->as.unary.operand = *place;
    deref->type = box ? box_deref(c, deref, type) : ty->inner;
    *place = deref;
    return deref->type;
}

TypeId auto_deref(Checker *c, Expr **place, TypeId type) {
    for (;;) {   // `Box<Box<T>>`, `&Box<T>`
        TypeId inner = deref_once(c, place, type);
        if (type_eq(inner, type)) {
            return type;
        }
        type = inner;
    }
}

TypeId method_receiver(Checker *c, Expr **place, TypeId type, Symbol name) {
    for (;;) {
        TypeId inner;
        bool through = type_get(c->types, type)->kind == TY_REF ||
                       (box_inner(c, type, &inner) && !has_method(c, c->sema->box, name));
        TypeId next = through ? deref_once(c, place, type) : type;
        if (type_eq(next, type)) {
            return type;
        }
        type = next;
    }
}

bool is_box_deref(Checker *c, const Expr *e) {
    TypeId inner;
    const Expr *operand = e->as.unary.operand;
    return operand->kind == EXPR_FIELD && box_inner(c, operand->as.field.base->type, &inner);
}
