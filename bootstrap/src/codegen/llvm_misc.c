// LLVM IR for select, str length and bytes, and the float functions.

#include "codegen/llvm_internal.h"

#include <string.h>

static const char *const FLOAT_FNS[FF_COUNT] = {
    [FF_SQRT] = "sqrt", [FF_FABS] = "fabs", [FF_FLOOR] = "floor",
    [FF_CEIL] = "ceil", [FF_MINNUM] = "minnum", [FF_MAXNUM] = "maxnum",
};

static void float_fn(Gen *g, const Rvalue *rv, Value *result) {
    bool binary = rv->fn == FF_MINNUM || rv->fn == FF_MAXNUM;
    Value a = gen_operand(g, rv->a);
    Value b = binary ? gen_operand(g, rv->b) : a;   // both loads come before the call
    bool is_f64 = strcmp(a.type, "double") == 0;
    g->float_fn_used[rv->fn][is_f64] = true;
    next_temp(g, result->text);
    out_fmt(&g->out, "  %s = call %s @llvm.%s.%s(%s %s", result->text, a.type, FLOAT_FNS[rv->fn],
            is_f64 ? "f64" : "f32", a.type, a.text);
    if (binary) {
        out_fmt(&g->out, ", %s %s", b.type, b.text);
    }
    out_str(&g->out, ")\n");
}

void gen_float_decls(Gen *g) {
    for (int fn = 0; fn < FF_COUNT; fn++) {
        for (int is_f64 = 0; is_f64 < 2; is_f64++) {
            if (!g->float_fn_used[fn][is_f64]) {
                continue;
            }
            const char *type = is_f64 ? "double" : "float";
            bool binary = fn == FF_MINNUM || fn == FF_MAXNUM;
            out_fmt(&g->out, "declare %s @llvm.%s.%s(%s%s%s)\n", type, FLOAT_FNS[fn],
                    is_f64 ? "f64" : "f32", type, binary ? ", " : "", binary ? type : "");
        }
    }
}

// A struct value: insertvalue of each field into poison.
static void aggregate(Gen *g, const Rvalue *rv, Value *result) {
    char current[16] = "poison";
    for (uint32_t i = 0; i < rv->count; i++) {
        Value field = gen_operand(g, rv->items[i]);
        if (strcmp(field.type, "void") == 0) {
            continue;   // a field of type (): nothing to store
        }
        char next[16];
        next_temp(g, next);
        out_fmt(&g->out, "  %s = insertvalue %s %s, %s %s, %u\n", next, result->type, current,
                field.type, field.text, i);
        memcpy(current, next, sizeof next);
    }
    memcpy(result->text, current, sizeof current);
}

void gen_misc(Gen *g, const Rvalue *rv, Value *result) {
    switch (rv->kind) {
    case RV_PTR_ADD: {
        Value p = gen_operand(g, rv->a), n = gen_operand(g, rv->b);
        const Ty *ptr = type_get(g->types, rv->a.type);
        next_temp(g, result->text);
        out_fmt(&g->out, "  %s = getelementptr %s, ptr %s, i64 %s\n", result->text,
                llvm_type(g, ptr->inner), p.text, n.text);
        break;
    }
    case RV_AGGREGATE: aggregate(g, rv, result); break;
    case RV_STR_LEN: {
        Value s = gen_operand(g, rv->a);
        next_temp(g, result->text);
        out_fmt(&g->out, "  %s = extractvalue %%astra.str %s, 1\n", result->text, s.text);
        break;
    }
    case RV_STR_BYTE: {
        Value s = gen_operand(g, rv->a), i = gen_operand(g, rv->b);
        char ptr[16], at[16];
        next_temp(g, ptr);
        out_fmt(&g->out, "  %s = extractvalue %%astra.str %s, 0\n", ptr, s.text);
        next_temp(g, at);
        out_fmt(&g->out, "  %s = getelementptr inbounds i8, ptr %s, i64 %s\n", at, ptr, i.text);
        next_temp(g, result->text);
        out_fmt(&g->out, "  %s = load i8, ptr %s\n", result->text, at);
        break;
    }
    default: float_fn(g, rv, result); break;
    }
}
