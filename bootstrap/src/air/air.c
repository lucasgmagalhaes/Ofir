#include "air/air.h"

static const char *const RUNTIME_NAMES[RT_COUNT] = {
    [RT_PRINT_STR] = "astra_print_str",
    [RT_PRINT_INT] = "astra_print_int",
    [RT_PRINT_UINT] = "astra_print_uint",
    [RT_PRINT_FLOAT] = "astra_print_float",
    [RT_PRINT_F32] = "astra_print_f32",
    [RT_PRINT_BOOL] = "astra_print_bool",
    [RT_PRINT_NEWLINE] = "astra_print_newline",
    [RT_STR_PUSH_STR] = "astra_str_push_str",
    [RT_STR_PUSH_INT] = "astra_str_push_int",
    [RT_STR_PUSH_UINT] = "astra_str_push_uint",
    [RT_STR_PUSH_FLOAT] = "astra_str_push_float",
    [RT_STR_PUSH_F32] = "astra_str_push_f32",
    [RT_STR_PUSH_BOOL] = "astra_str_push_bool",
    [RT_STR_CONCAT] = "astra_str_concat",
    [RT_STR_EQ] = "astra_str_eq",
    [RT_STR_CMP] = "astra_str_cmp",
};

const char *runtime_fn_name(RuntimeFn fn) {
    return RUNTIME_NAMES[fn];
}

BlockId air_new_block(AirBody *body) {
    VEC_PUSH(body->blocks, (AirBlock){0});
    return (BlockId){(uint32_t)body->blocks.len - 1};
}

LocalId air_new_local(AirBody *body, TypeId type, Symbol name, Span span) {
    VEC_PUSH(body->locals, ((AirLocal){.type = type, .name = name, .span = span}));
    return (LocalId){(uint32_t)body->locals.len - 1};
}

void air_push(AirBody *body, BlockId block, AirStmt stmt) {
    VEC_PUSH(air_block(body, block)->stmts, stmt);
}

void air_terminate(AirBody *body, BlockId block, Terminator term) {
    air_block(body, block)->term = term;
}

void air_program_free(AirProgram *program) {
    for (size_t i = 0; i < program->bodies.len; i++) {
        AirBody *body = &program->bodies.data[i];
        for (size_t b = 0; b < body->blocks.len; b++) {
            VEC_FREE(body->blocks.data[b].stmts);
        }
        VEC_FREE(body->blocks);
        VEC_FREE(body->locals);
    }
    VEC_FREE(program->bodies);
}
