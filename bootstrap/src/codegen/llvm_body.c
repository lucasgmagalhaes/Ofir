// LLVM IR for function bodies: locals, statements and terminators.

#include "codegen/llvm_internal.h"

#include <string.h>

static void store_at(Gen *g, LocalId dest, FieldPath path, Value v) {
    if (strcmp(v.type, "void") != 0) {
        char at[16];
        place_address(g, dest, path, at);
        out_fmt(&g->out, "  store %s %s, ptr %s\n", v.type, v.text, at);
    }
}

static void store(Gen *g, LocalId dest, Value v) {
    store_at(g, dest, (FieldPath){0}, v);
}

// Runtime arguments: bools are `zeroext i1` in the C ABI.
static void runtime_call(Gen *g, const AirStmt *s) {
    Value args[4];
    for (uint32_t i = 0; i < s->as.call.arg_count; i++) {
        args[i] = gen_operand(g, s->as.call.args[i]);
    }
    RuntimeFn fn = s->as.call.rt;
    bool returns_bool = fn == RT_STR_EQ || fn == RT_STR_STARTS_WITH || fn == RT_STR_ENDS_WITH ||
                        fn == RT_STR_CONTAINS;
    const char *ret = returns_bool ? "i1" : fn == RT_STR_CMP ? "i32" : "void";
    char result[16] = "";
    if (s->has_dest) {
        next_temp(g, result);
        out_fmt(&g->out, "  %s = ", result);
    } else {
        out_str(&g->out, "  ");
    }
    out_fmt(&g->out, "call %s @%s(", ret, runtime_fn_name(fn));
    for (uint32_t i = 0; i < s->as.call.arg_count; i++) {
        bool is_bool = strcmp(args[i].type, "i1") == 0;
        out_fmt(&g->out, "%s%s%s %s", i ? ", " : "", args[i].type, is_bool ? " zeroext" : "",
                args[i].text);
    }
    out_str(&g->out, ")\n");
    if (s->has_dest) {
        Value v = {.type = ret};
        memcpy(v.text, result, sizeof result);
        store(g, s->dest, v);
    }
}

static void user_call(Gen *g, const AirStmt *s) {
    Value args[16];
    uint32_t count = s->as.call.arg_count;
    Value *values = count <= 16 ? args : ARENA_ARRAY(&g->sema->session->arena, Value, count);
    for (uint32_t i = 0; i < count; i++) {
        values[i] = gen_operand(g, s->as.call.args[i]);
    }
    const Ty *sig = type_get(g->types, sema_def(g->sema, s->as.call.fn)->type);
    const char *ret = llvm_type(g, sig->inner);
    char result[16] = "";
    if (s->has_dest) {
        next_temp(g, result);
        out_fmt(&g->out, "  %s = call fastcc %s ", result, ret);
    } else {
        out_fmt(&g->out, "  call fastcc %s ", ret);
    }
    function_name(g, &g->out, s->as.call.fn, false);
    out_char(&g->out, '(');
    bool first = true;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(values[i].type, "void") != 0) {   // () arguments are not passed
            out_fmt(&g->out, "%s%s %s", first ? "" : ", ", values[i].type, values[i].text);
            first = false;
        }
    }
    out_str(&g->out, ")\n");
    if (s->has_dest) {
        Value v = {.type = ret};
        memcpy(v.text, result, sizeof result);
        store(g, s->dest, v);
    }
}

static void statement(Gen *g, const AirStmt *s) {
    if (s->kind == AIR_CALL) {
        if (s->as.call.runtime) {
            runtime_call(g, s);
        } else {
            user_call(g, s);
        }
        return;
    }
    Value v;
    TypeId type = g->body->locals.data[s->dest.id].type;
    if (s->dest_path.len > 0) {   // the value has the field's type: the operand's
        type = s->as.rvalue.a.type;
    }
    gen_rvalue(g, &s->as.rvalue, type, &v);
    store_at(g, s->dest, s->dest_path, v);
}

static const char *const PANIC_FNS[] = {
    [PANIC_OVERFLOW] = "astra_panic_overflow", [PANIC_DIV_ZERO] = "astra_panic_div_zero",
    [PANIC_SHIFT] = "astra_panic_shift", [PANIC_CAST] = "astra_panic_cast",
    [PANIC_BOUNDS] = "astra_panic_bounds", [PANIC_ASSERT] = "astra_panic_assert",
};

// The failing branch calls the panic function with the source location.
static void assertion(Gen *g, const Terminator *t, size_t index) {
    Value c = gen_operand(g, t->cond);
    uint32_t loc = location_global(g, t->span);
    out_fmt(&g->out, "  br i1 %s, label %%panic%zu, label %%bb%u\n", c.text, index, t->target.id);
    out_fmt(&g->out, "panic%zu:\n", index);
    if (t->panic == PANIC_BOUNDS) {   // the message shows the index and the length
        Value i = gen_operand(g, t->index), len = gen_operand(g, t->len);
        out_fmt(&g->out, "  call void @astra_panic_bounds(i64 %s, i64 %s, ptr @.loc.%u)\n",
                i.text, len.text, loc);
    } else {
        out_fmt(&g->out, "  call void @%s(ptr @.loc.%u)\n", PANIC_FNS[t->panic], loc);
    }
    out_str(&g->out, "  unreachable\n");
}

static void return_value(Gen *g) {
    TypeId ret = g->body->locals.data[0].type;
    if (llvm_has_value(ret)) {
        Value v = gen_operand(g, (Operand){.kind = OPERAND_COPY, .type = ret});
        out_fmt(&g->out, "  ret %s %s\n", v.type, v.text);
    } else {
        out_str(&g->out, "  ret void\n");
    }
}

static void terminator(Gen *g, const Terminator *t, size_t index) {
    switch (t->kind) {
    case TERM_GOTO: out_fmt(&g->out, "  br label %%bb%u\n", t->target.id); break;
    case TERM_BRANCH: {
        Value c = gen_operand(g, t->cond);
        out_fmt(&g->out, "  br i1 %s, label %%bb%u, label %%bb%u\n", c.text, t->target.id,
                t->other.id);
        break;
    }
    case TERM_ASSERT: assertion(g, t, index); break;
    case TERM_PANIC: {
        Value msg = gen_operand(g, t->cond);
        uint32_t loc = location_global(g, t->span);
        out_fmt(&g->out, "  call void @astra_panic_str(ptr %s, ptr @.loc.%u)\n  unreachable\n",
                msg.text, loc);
        break;
    }
    case TERM_RETURN: return_value(g); break;
    case TERM_UNREACHABLE: case TERM_NONE: out_str(&g->out, "  unreachable\n"); break;
    }
}

static void header(Gen *g, const AirBody *body) {
    TypeId ret = body->locals.data[0].type;
    out_fmt(&g->out, "define %s%s ", body->is_main ? "" : "internal fastcc ",
            body->is_main ? "void" : llvm_type(g, ret));
    function_name(g, &g->out, body->def, body->is_main);
    out_char(&g->out, '(');
    bool first = true;
    for (uint32_t i = 1; i <= body->param_count; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            out_fmt(&g->out, "%s%s %%p%u", first ? "" : ", ", llvm_type(g, type), i);
            first = false;
        }
    }
    out_str(&g->out, ") nounwind {\nstart:\n");
}

void gen_body(Gen *g, const AirBody *body) {
    g->body = body;
    g->temp = 0;
    header(g, body);
    for (size_t i = 0; i < body->locals.len; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            out_fmt(&g->out, "  %%l%zu = alloca %s\n", i, llvm_type(g, type));
        }
    }
    for (uint32_t i = 1; i <= body->param_count; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            out_fmt(&g->out, "  store %s %%p%u, ptr %%l%u\n", llvm_type(g, type), i, i);
        }
    }
    out_str(&g->out, "  br label %bb0\n");
    for (size_t b = 0; b < body->blocks.len; b++) {
        const AirBlock *block = &body->blocks.data[b];
        out_fmt(&g->out, "bb%zu:\n", b);
        for (size_t i = 0; i < block->stmts.len; i++) {
            statement(g, &block->stmts.data[i]);
        }
        terminator(g, &block->term, b);
    }
    out_str(&g->out, "}\n\n");
}
