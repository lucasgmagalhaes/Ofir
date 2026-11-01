// `List<T>` (docs/std/collections.md): the parts the compiler provides on
// top of `std/core/list.at` — literals, indexing and `for` loops.

#include "sema/check_internal.h"

bool sequence_elem(Checker *c, TypeId type, TypeId *elem) {
    const Ty *ty = type_get(c->types, type);
    if (c->sema->slice.id != 0 && ty->kind == TY_ADT && ty->data == c->sema->slice.id) {
        *elem = ty->args[0];
        return true;
    }
    return list_elem(c, type, elem);
}

bool list_elem(Checker *c, TypeId type, TypeId *elem) {
    const Ty *ty = type_get(c->types, type);
    if (c->sema->list.id == 0 || ty->kind != TY_ADT || ty->data != c->sema->list.id ||
        ty->arg_count != 1) {
        return false;
    }
    *elem = ty->args[0];
    return true;
}

static TypeId list_of(Checker *c, TypeId elem) {
    return type_intern(c->types, (Ty){.kind = TY_ADT, .data = c->sema->list.id, .arg_count = 1,
                                      .args = &elem});
}

void require_copy(Checker *c, Span span, TypeId type, const char *what) {
    if (!type_is_copy(c, type) && type.id != TID_ERROR) {
        check_error(c, "E0321", span, "%s needs a value of a Copy type, not `%s`", what,
                    type_name(c, type));
    }
}

// `[a, b, c]` and `[]`: the elements have one type, from the expected
// `List<T>` or the first element.
TypeId check_list_literal(Checker *c, Expr *e, TypeId expected) {
    if (c->sema->list.id == 0) {
        return unsupported(c, e->span, "list literals without the standard library are");
    }
    TypeId elem = NO_TYPE;
    bool known = list_elem(c, expected, &elem);
    ExprList items = e->as.elems;
    for (uint32_t i = 0; i < items.len; i++) {
        if (known) {
            coerce(c, items.items[i], elem);
        } else {
            elem = check_expr(c, items.items[i], NO_TYPE);
            known = true;
        }
    }
    if (!known) {
        check_error(c, "E0330", e->span, "cannot infer the element type of `[]`: write the "
                    "type, as in `let names: List<str> = []`");
        return type_id(TID_ERROR);
    }
    TypeId type = list_of(c, elem);
    check_instance_storable(c, type, e->span);
    return type;
}

// `[value; count]`: `count` copies of a Copy value.
TypeId check_list_repeat(Checker *c, Expr *e, TypeId expected) {
    if (c->sema->list.id == 0) {
        return unsupported(c, e->span, "list literals without the standard library are");
    }
    TypeId elem;
    TypeId hint = list_elem(c, expected, &elem) ? elem : NO_TYPE;
    elem = check_expr(c, e->as.repeat.value, hint);
    require_copy(c, e->as.repeat.value->span, elem, "`[value; count]`");
    coerce(c, e->as.repeat.count, type_id(TID_INT));
    return list_of(c, elem);
}

// `for x in list` or `for x in slice`: `x` borrows each element (spec 5.5);
// stage 0 binds it to a copy of the bits, which is the same while there are
// no drops (roadmap 3.2).
TypeId check_for_list(Checker *c, Expr **list) {
    TypeId type = check_expr(c, *list, NO_TYPE);
    type = auto_deref(c, list, type);
    TypeId elem;
    if (!sequence_elem(c, type, &elem)) {
        if (type.id != TID_ERROR) {
            check_error(c, "E0300", (*list)->span, "mismatched types: `for` needs a range, a "
                        "list or a slice, found %s", type_name(c, type));
        }
        return type_id(TID_ERROR);
    }
    return elem;
}

// `xs[a..b]`, `xs[a..]`, `xs[..b]`: rewritten to `xs.slice(a, b)` or
// `xs.sliceFrom(a)`, methods of List, Slice and str.
TypeId check_range_index(Checker *c, Expr *e) {
    Arena *arena = &c->sema->session->arena;
    Expr *base = e->as.index.base, *range = e->as.index.index;
    Expr *lo = range->as.range.lo;
    if (!lo) {
        lo = ARENA_NEW(arena, Expr);
        *lo = (Expr){.kind = EXPR_INT, .span = range->span};
    }
    Expr *hi = range->as.range.hi;
    if (hi && range->as.range.inclusive) {   // `a..=b` is `a..b + 1`
        Expr *one = ARENA_NEW(arena, Expr);
        *one = (Expr){.kind = EXPR_INT, .span = hi->span, .as.int_value = 1};
        Expr *sum = ARENA_NEW(arena, Expr);
        *sum = (Expr){.kind = EXPR_BINARY, .span = hi->span};
        sum->as.binary.op = OP_ADD;
        sum->as.binary.lhs = hi;
        sum->as.binary.rhs = one;
        hi = sum;
    }
    uint32_t count = hi ? 2 : 1;
    Arg *args = ARENA_ARRAY(arena, Arg, 2);
    args[0] = (Arg){.value = lo};
    args[1] = (Arg){.value = hi};
    Expr *field = ARENA_NEW(arena, Expr);
    *field = (Expr){.kind = EXPR_FIELD, .span = e->span};
    field->as.field.base = base;
    field->as.field.name = intern(&c->sema->session->names, hi ? SV("slice") : SV("sliceFrom"));
    e->kind = EXPR_CALL;
    e->as.call.callee = field;
    e->as.call.args = (ArgList){.items = args, .len = count};
    e->as.call.type_args = (TypeId){0};
    return check_method_call(c, e, NO_TYPE);
}

// Stage 0 sorts the elements it can compare without bounds (roadmap 5.2).
void check_list_sort(Checker *c, Expr *call) {
    const Ty *args = type_get(c->types, call->as.call.type_args);
    TypeId elem = args->arg_count > 0 ? args->args[0] : type_id(TID_ERROR);
    TyKind kind = type_get(c->types, elem)->kind;
    if (kind != TY_INT && kind != TY_FLOAT && kind != TY_STR && kind != TY_ERROR) {
        check_error(c, "E0313", call->span, "`sort` needs elements of type int, float or str "
                    "in stage 0, not `%s`", type_name(c, elem));
    }
}

// The fields the compiler reads: `ptr: *mut T`, `len: int`, then `cap`.
void check_list_layout(Checker *c, const Def *def) {
    const StructInfo *info = def->fields;
    const Ty *ptr = info && info->count == 3 ? type_get(c->types, info->types[0]) : NULL;
    if (!ptr || ptr->kind != TY_PTR || info->types[1].id != TID_INT) {
        check_error(c, "E0806", def->span,
                    "`@lang(\"list\")` must declare `ptr: *mut T`, `len: int`, `cap: int`");
    }
}

static FnDecl *list_method(Checker *c, const char *name) {
    FnList methods = sema_def(c->sema, c->sema->list)->item->as.struct_.methods;
    for (uint32_t i = 0; i < methods.len; i++) {
        if (sv_eq(symbol_name(&c->sema->session->names, methods.items[i]->name),
                  sv_from_cstr(name))) {
            return methods.items[i];
        }
    }
    return NULL;
}

// A `List<T>` where a `&[T]` is expected (spec 2.12) becomes `list.asSlice()`.
bool coerce_to_slice(Checker *c, Expr *e, TypeId found, TypeId expected) {
    TypeId elem, wanted;
    const Ty *ty = type_get(c->types, expected);
    if (ty->kind != TY_ADT || ty->data != c->sema->slice.id || c->sema->slice.id == 0 ||
        !list_elem(c, found, &elem) || !type_eq(elem, ty->args[0])) {
        return false;
    }
    wanted = expected;
    FnDecl *as_slice = list_method(c, "asSlice");
    if (!as_slice) {
        return false;
    }
    fn_signature(c, as_slice);
    Arena *arena = &c->sema->session->arena;
    Expr *list = ARENA_NEW(arena, Expr);
    *list = *e;
    Expr *field = ARENA_NEW(arena, Expr);
    *field = (Expr){.kind = EXPR_FIELD, .span = e->span};
    field->as.field.base = list;
    field->as.field.name = as_slice->name;
    field->as.field.res = (Res){RES_DEF, as_slice->def.id};
    *e = (Expr){.kind = EXPR_CALL, .span = e->span, .type = wanted};
    e->as.call.callee = field;
    e->as.call.type_args = type_tuple(c->types, &elem, 1);
    return true;
}
