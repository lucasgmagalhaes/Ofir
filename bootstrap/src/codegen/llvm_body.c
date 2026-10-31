// LLVM IR for function bodies: locals, statements and terminators.

#include "codegen/llvm_internal.h"

#include <string.h>

static void store(Gen *g, LocalId dest, Value v) {
    if (strcmp(v.type, "void") != 0) {
        fprintf(g->out, "  store %s %s, ptr %%l%u\n", v.type, v.text, dest.id);
    }
}

// Runtime arguments: bools are `zeroext i1` in the C ABI.
static void runtime_call(Gen *g, const AirStmt *s) {
    Value args[4];
    for (uint32_t i = 0; i < s->as.call.arg_count; i++) {
        args[i] = gen_operand(g, s->as.call.args[i]);
    }
    RuntimeFn fn = s->as.call.rt;
    const char *ret = fn == RT_STR_EQ ? "i1" : fn == RT_STR_CMP ? "i32" : "void";
    char result[16] = "";
    if (s->has_dest) {
        next_temp(g, result);
        fprintf(g->out, "  %s = ", result);
    } else {
        fputs("  ", g->out);
    }
    fprintf(g->out, "call %s @%s(", ret, runtime_fn_name(fn));
    for (uint32_t i = 0; i < s->as.call.arg_count; i++) {
        bool is_bool = strcmp(args[i].type, "i1") == 0;
        fprintf(g->out, "%s%s%s %s", i ? ", " : "", args[i].type, is_bool ? " zeroext" : "",
                args[i].text);
    }
    fputs(")\n", g->out);
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
    StrBuf name = {0};
    function_name(g, &name, s->as.call.fn, false);
    char result[16] = "";
    if (s->has_dest) {
        next_temp(g, result);
        fprintf(g->out, "  %s = call fastcc %s %s(", result, ret, name.data);
    } else {
        fprintf(g->out, "  call fastcc %s %s(", ret, name.data);
    }
    bool first = true;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(values[i].type, "void") != 0) {   // () arguments are not passed
            fprintf(g->out, "%s%s %s", first ? "" : ", ", values[i].type, values[i].text);
            first = false;
        }
    }
    fputs(")\n", g->out);
    strbuf_free(&name);
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
    gen_rvalue(g, &s->as.rvalue, g->body->locals.data[s->dest.id].type, &v);
    store(g, s->dest, v);
}

static const char *const PANIC_FNS[] = {
    [PANIC_OVERFLOW] = "astra_panic_overflow", [PANIC_DIV_ZERO] = "astra_panic_div_zero",
    [PANIC_SHIFT] = "astra_panic_shift", [PANIC_CAST] = "astra_panic_cast",
};

// The failing branch calls the panic function with the source location.
static void assertion(Gen *g, const Terminator *t, size_t index) {
    Value c = gen_operand(g, t->cond);
    uint32_t loc = location_global(g, t->span);
    fprintf(g->out, "  br i1 %s, label %%panic%zu, label %%bb%u\n", c.text, index, t->target.id);
    fprintf(g->out, "panic%zu:\n  call void @%s(ptr @.loc.%u)\n  unreachable\n", index,
            PANIC_FNS[t->panic], loc);
}

static void return_value(Gen *g) {
    TypeId ret = g->body->locals.data[0].type;
    if (llvm_has_value(ret)) {
        Value v = gen_operand(g, (Operand){.kind = OPERAND_COPY, .type = ret});
        fprintf(g->out, "  ret %s %s\n", v.type, v.text);
    } else {
        fputs("  ret void\n", g->out);
    }
}

static void terminator(Gen *g, const Terminator *t, size_t index) {
    switch (t->kind) {
    case TERM_GOTO: fprintf(g->out, "  br label %%bb%u\n", t->target.id); break;
    case TERM_BRANCH: {
        Value c = gen_operand(g, t->cond);
        fprintf(g->out, "  br i1 %s, label %%bb%u, label %%bb%u\n", c.text, t->target.id,
                t->other.id);
        break;
    }
    case TERM_ASSERT: assertion(g, t, index); break;
    case TERM_RETURN: return_value(g); break;
    case TERM_UNREACHABLE: case TERM_NONE: fputs("  unreachable\n", g->out); break;
    }
}

static void header(Gen *g, const AirBody *body) {
    StrBuf name = {0};
    function_name(g, &name, body->def, body->is_main);
    TypeId ret = body->locals.data[0].type;
    fprintf(g->out, "define %s%s %s(", body->is_main ? "" : "internal fastcc ",
            body->is_main ? "void" : llvm_type(g, ret), name.data);
    bool first = true;
    for (uint32_t i = 1; i <= body->param_count; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            fprintf(g->out, "%s%s %%p%u", first ? "" : ", ", llvm_type(g, type), i);
            first = false;
        }
    }
    fputs(") nounwind {\nstart:\n", g->out);
    strbuf_free(&name);
}

void gen_body(Gen *g, const AirBody *body) {
    g->body = body;
    g->temp = 0;
    header(g, body);
    for (size_t i = 0; i < body->locals.len; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            fprintf(g->out, "  %%l%zu = alloca %s\n", i, llvm_type(g, type));
        }
    }
    for (uint32_t i = 1; i <= body->param_count; i++) {
        TypeId type = body->locals.data[i].type;
        if (llvm_has_value(type)) {
            fprintf(g->out, "  store %s %%p%u, ptr %%l%u\n", llvm_type(g, type), i, i);
        }
    }
    fputs("  br label %bb0\n", g->out);
    for (size_t b = 0; b < body->blocks.len; b++) {
        const AirBlock *block = &body->blocks.data[b];
        fprintf(g->out, "bb%zu:\n", b);
        for (size_t i = 0; i < block->stmts.len; i++) {
            statement(g, &block->stmts.data[i]);
        }
        terminator(g, &block->term, b);
    }
    fputs("}\n\n", g->out);
}
