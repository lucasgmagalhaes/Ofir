// `ofir test` (spec 12.2, roadmap 3.11). The roots are the `@test`
// functions of the root module and of the modules outside std, and the
// program's entry point is generated: it runs the test whose index
// `ofir_test_index()` returns (the driver passes it as the argument), so
// that each test runs in a process of its own.

#include "air/lower_internal.h"

void find_tests(Lowerer *l, uint32_t module) {
    ItemList items = l->sema->program.items[module].module->items;
    for (uint32_t i = 0; i < items.len; i++) {
        Item *item = items.items[i];
        if (item->kind == ITEM_FN && item->as.fn->body &&
            find_attribute(l->sema, item->attributes, "test") && !skipped_test(l->sema, item)) {
            VEC_PUSH(l->sema->tests, item->as.fn);
        }
    }
}

// `ofir_main`: `if index == 0 { test0() } else if index == 1 { ... }`.
void lower_test_main(Lowerer *l, BodySink sink, void *context) {
    AirBody body = {.def = {0}, .targs = type_id(TID_UNIT), .is_main = true};
    l->body = &body;
    l->fn = NULL;
    l->flag_count = 0;
    air_new_local(&body, type_id(TID_UNIT), (Symbol){0}, (Span){0});
    l->block = air_new_block(&body);
    TypeId int_type = type_id(TID_INT), bool_type = type_id(TID_BOOL);
    LocalId index = new_temp(l, int_type);
    emit_runtime(l, RT_TEST_INDEX, NULL, 0, &index);
    BlockId done = air_new_block(&body);
    for (size_t k = 0; k < l->sema->tests.len; k++) {
        Rvalue test = {.kind = RV_BINARY, .op = OP_EQ, .a = op_copy(index, int_type),
                       .b = op_int(k, int_type)};
        Operand chosen = op_copy(emit_temp(l, bool_type, test), bool_type);
        BlockId run = air_new_block(&body), next = air_new_block(&body);
        terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = chosen, .target = run,
                                  .other = next});
        switch_to(l, run);
        emit_direct_call(l, l->sema->tests.data[k], type_id(TID_UNIT), (CallArgs){NULL, 0}, NULL);
        emit_goto(l, done);
        switch_to(l, next);
    }
    emit_goto(l, done);
    switch_to(l, done);
    air_terminate(&body, l->block, (Terminator){.kind = TERM_RETURN});
    mono_body(l, &body);
    sink(context, &body);
    air_body_free(&body);
}
