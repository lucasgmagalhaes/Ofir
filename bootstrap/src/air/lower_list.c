// Lowering of `List<T>` literals, indexing and `for` loops. A literal calls
// the functions of `std/core/list.at`; an element is reached through the
// list's `ptr` and `len` fields (positions 0 and 1, checked by
// check_list_layout).

#include "air/lower_internal.h"

enum { LIST_PTR = 0, LIST_LEN = 1 };

bool is_list(Lowerer *l, TypeId type) {
    const Ty *ty = type_get(l->types, type);
    return ty->kind == TY_ADT && ty->data == l->sema->list.id && l->sema->list.id != 0;
}

static TypeId elem_of(Lowerer *l, TypeId list) {
    return type_get(l->types, list)->args[0];
}

static Place field_of(Lowerer *l, Place place, uint32_t field) {
    uint32_t *fields = ARENA_ARRAY(&l->sema->session->arena, uint32_t, place.path.len + 1);
    for (uint32_t i = 0; i < place.path.len; i++) {
        fields[i] = place.path.fields[i];
    }
    fields[place.path.len] = field;
    place.path.fields = fields;
    place.path.len++;
    return place;
}

typedef struct {
    Operand *items;
    uint32_t count;
} Args;

// Calls `List<elem>.name(args)`, which mono_body reaches; `dest` receives
// the result unless it is NULL.
static void call_list(Lowerer *l, const char *name, TypeId elem, Args args, LocalId *dest) {
    FnList methods = sema_def(l->sema, l->sema->list)->item->as.struct_.methods;
    StrView wanted = sv_from_cstr(name);
    FnDecl *fn = NULL;
    for (uint32_t i = 0; i < methods.len && !fn; i++) {
        if (sv_eq(symbol_name(&l->sema->session->names, methods.items[i]->name), wanted)) {
            fn = methods.items[i];
        }
    }
    if (!fn) {
        fatal("lowering: std/core/list.at has no `%s`", name);
    }
    AirStmt s = {.kind = AIR_CALL, .has_dest = dest != NULL};
    if (dest) {
        s.dest = *dest;
    }
    s.as.call.fn = fn->def;
    s.as.call.targs = type_tuple(l->types, &elem, 1);
    s.as.call.args = args.items;
    s.as.call.arg_count = args.count;
    air_push(l->body, l->block, s);
}

// `[a, b, c]`: `withCapacity(3)`, then `push` of each element in order;
// `[]` is `new()`.
Operand lower_list_literal(Lowerer *l, Expr *e) {
    TypeId elem = elem_of(l, e->type);
    ExprList items = e->as.elems;
    LocalId list = new_temp(l, e->type);
    Arena *arena = &l->sema->session->arena;
    if (items.len == 0) {
        call_list(l, "new", elem, (Args){0}, &list);
        return op_copy(list, e->type);
    }
    Operand *capacity = ARENA_NEW(arena, Operand);
    *capacity = op_int(items.len, type_id(TID_INT));
    call_list(l, "withCapacity", elem, (Args){capacity, 1}, &list);
    Operand self = place_operand((Place){.local = list}, OPERAND_ADDR, e->type);
    for (uint32_t i = 0; i < items.len; i++) {
        Operand *args = ARENA_ARRAY(arena, Operand, 2);
        args[0] = self;
        args[1] = lower_expr(l, items.items[i]);
        call_list(l, "push", elem, (Args){args, 2}, NULL);
    }
    return op_copy(list, e->type);
}

// `[value; count]`: `filled(value, count)`.
Operand lower_list_repeat(Lowerer *l, Expr *e) {
    Operand *args = ARENA_ARRAY(&l->sema->session->arena, Operand, 2);
    args[0] = lower_expr(l, e->as.repeat.value);
    args[0] = settle(l, args[0], e->as.repeat.count);
    args[1] = lower_expr(l, e->as.repeat.count);
    LocalId list = new_temp(l, e->type);
    call_list(l, "filled", elem_of(l, e->type), (Args){args, 2}, &list);
    return op_copy(list, e->type);
}

// The address of element `index` of the list at `list`, after the check.
static LocalId element(Lowerer *l, Place list, TypeId type, Operand index, Span span) {
    Operand len = place_operand(field_of(l, list, LIST_LEN), OPERAND_COPY, type_id(TID_INT));
    emit_bounds_check(l, index, len, span);
    TypeId ptr = struct_field_type(l->sema, type, LIST_PTR);
    Operand base = place_operand(field_of(l, list, LIST_PTR), OPERAND_COPY, ptr);
    return emit_temp(l, ptr, (Rvalue){.kind = RV_PTR_ADD, .a = base, .b = index});
}

Place lower_list_index(Lowerer *l, Expr *e) {
    Place list = lower_place(l, e->as.index.base);
    Operand index = lower_expr(l, e->as.index.index);
    index = op_copy(to_local(l, index), type_id(TID_INT));
    LocalId at = element(l, list, e->as.index.base->type, index, e->span);
    return (Place){.local = at, .path = {.deref = true}};
}

// `for x in list`: a hidden counter; the list's length and buffer are read
// at each iteration, and `x` is a copy of the element.
void lower_for_list(Lowerer *l, Stmt *s) {
    Expr *expr = s->as.for_.expr;
    Place list = lower_place(l, expr);
    TypeId int_type = type_id(TID_INT);
    LocalId counter = emit_temp(l, int_type, (Rvalue){.kind = RV_USE, .a = op_int(0, int_type)});
    Operand current = op_copy(counter, int_type);
    BlockId header = air_new_block(l->body), body = air_new_block(l->body);
    BlockId latch = air_new_block(l->body), exit = air_new_block(l->body);
    emit_goto(l, header);
    switch_to(l, header);
    Operand len = place_operand(field_of(l, list, LIST_LEN), OPERAND_COPY, int_type);
    Rvalue test = {.kind = RV_BINARY, .op = OP_LT, .a = current, .b = len};
    Operand go = op_copy(emit_temp(l, type_id(TID_BOOL), test), type_id(TID_BOOL));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = go, .target = body, .other = exit});
    switch_to(l, body);
    TypeId ptr = struct_field_type(l->sema, expr->type, LIST_PTR);
    Operand base = place_operand(field_of(l, list, LIST_PTR), OPERAND_COPY, ptr);
    LocalId at = emit_temp(l, ptr, (Rvalue){.kind = RV_PTR_ADD, .a = base, .b = current});
    Operand value = place_operand((Place){.local = at, .path = {.deref = true}}, OPERAND_COPY,
                                  elem_of(l, expr->type));
    emit_assign(l, air_local_of(s->as.for_.first_local), (Rvalue){.kind = RV_USE, .a = value});
    lower_loop_body(l, s->as.for_.body, latch, exit);
    switch_to(l, latch);   // counter < len: the increment cannot overflow
    emit_assign(l, counter, (Rvalue){.kind = RV_BINARY, .op = OP_ADD, .a = current,
                                     .b = op_int(1, int_type)});
    emit_goto(l, header);
    switch_to(l, exit);
}
