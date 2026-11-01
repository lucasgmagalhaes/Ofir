// `for x in iterator` (roadmap 3.7, sema/check_iter.c): `next` is called
// on the iterator until it returns `None`. A temporary iterator lives until
// the end of the `for` statement; a variable is advanced in place. `x` owns
// each item and drops it at the end of its iteration.

#include "air/lower_internal.h"

bool is_iterator(Lowerer *l, TypeId type) {
    type = concrete_type(l, type);
    return type_get(l->types, type)->kind == TY_ADT && type_method(l, type, SV("next")) != NULL;
}

// `it.next()` into a new Option temporary.
static LocalId call_next(Lowerer *l, Place iter, TypeId type, TypeId option) {
    FnDecl *next = type_method(l, type, SV("next"));
    Operand *self = ARENA_NEW(&l->sema->session->arena, Operand);
    *self = place_operand(iter, OPERAND_ADDR, type_ref(l->types, type, true));
    LocalId dest = new_temp(l, option);
    emit_direct_call(l, next, method_targs(l, type), (CallArgs){self, 1}, &dest);
    return dest;
}

void lower_for_iter(Lowerer *l, Stmt *s) {
    Expr *expr = s->as.for_.expr;
    TypeId type = concrete_type(l, expr->type);
    Place iter = lower_place(l, expr);
    LocalId item = air_local_of(s->as.for_.first_local);
    TypeId option = type_intern(l->types, (Ty){.kind = TY_ADT, .data = l->sema->option.id,
                                               .arg_count = 1,
                                               .args = &l->body->locals.data[item.id].type});
    option = concrete_type(l, option);
    BlockId header = air_new_block(l->body), body = air_new_block(l->body);
    BlockId latch = air_new_block(l->body), exit = air_new_block(l->body);
    emit_goto(l, header);
    switch_to(l, header);
    Place next = {.local = call_next(l, iter, type, option)};
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = is_variant(l, next, option, 0),
                              .target = body, .other = exit});   // Some
    switch_to(l, body);
    TypeId elem = type_get(l->types, option)->args[0];
    emit_assign(l, item, (Rvalue){.kind = RV_USE,
                                  .a = place_operand(variant_field(l, next, 0, 0),
                                                     OPERAND_COPY, elem)});
    drop_scope_enter(l);   // the item, dropped at the end of each iteration
    own_local(l, item, true);
    VEC_PUSH(l->loops, ((Loop){.break_to = exit, .continue_to = latch,
                              .scopes = (uint32_t)l->scopes.len - 1,
                              .temp_scopes = (uint32_t)l->temp_scopes.len}));
    lower_block(l, s->as.for_.body, false);
    l->loops.len--;
    drop_scope_leave(l);
    emit_goto(l, latch);
    switch_to(l, latch);
    emit_goto(l, header);
    switch_to(l, exit);
}
