// `List<T>` (docs/std/collections.md): the parts the compiler provides on
// top of `std/core/list.at` — literals, indexing and `for` loops.

#include "sema/check_internal.h"

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
    return list_of(c, elem);
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

// `for x in list`: `x` is a copy of each element, so the elements must be
// Copy until borrowing iterators (roadmap 2.21).
TypeId check_for_list(Checker *c, Expr *list) {
    TypeId type = check_expr(c, list, NO_TYPE);
    TypeId elem;
    if (!list_elem(c, type, &elem)) {
        if (type.id != TID_ERROR) {
            check_error(c, "E0300", list->span, "mismatched types: `for` needs a range or a "
                        "list, found %s", type_name(c, type));
        }
        return type_id(TID_ERROR);
    }
    require_copy(c, list->span, elem, "`for x in list`");
    return elem;
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
