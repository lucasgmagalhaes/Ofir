// LLVM IR generation: types, names, globals and the module.

#include "codegen/llvm_internal.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

// A struct is a named LLVM type, `%"Point"`, defined at its first use.
static const char *struct_type(Gen *g, uint32_t def) {
    if (def >= g->adt_count) {
        size_t count = g->sema->defs.len;
        g->adt_types = xrealloc(g->adt_types, count * sizeof(char *));
        for (size_t i = g->adt_count; i < count; i++) {
            g->adt_types[i] = NULL;
        }
        g->adt_count = count;
    }
    if (g->adt_types[def]) {
        return g->adt_types[def];
    }
    StrView name = def_name(g, (DefId){def});
    char *text = ARENA_ARRAY(&g->sema->session->arena, char, name.len + 16);
    Out out;
    out_to_array(&out, text, name.len + 16);
    out_str(&out, "%\"");
    out_write(&out, name.ptr, name.len);
    out_fmt(&out, ".%u\"", def);   // the DefId keeps types of the same name apart
    g->adt_types[def] = text;
    const StructInfo *info = sema_def(g->sema, (DefId){def})->fields;
    for (uint32_t i = 0; info && i < info->count; i++) {
        llvm_type(g, info->types[i]);   // define the field types first: no interleaving
    }
    out_fmt(&g->type_defs, "%s = type { ", text);
    for (uint32_t i = 0; info && i < info->count; i++) {
        const char *field = llvm_type(g, info->types[i]);
        out_fmt(&g->type_defs, "%s%s", i ? ", " : "", strcmp(field, "void") ? field : "{}");
    }
    out_str(&g->type_defs, " }\n");
    return text;
}

// check-sizes: table
const char *llvm_type(Gen *g, TypeId type) {
    const Ty *ty = type_get(g->types, type);
    switch (ty->kind) {
    case TY_ADT: return struct_type(g, ty->data);
    case TY_BOOL: return "i1";
    case TY_INT:
        switch (ty->data) {
        case INT_I8: case INT_U8: return "i8";
        case INT_I16: case INT_U16: return "i16";
        case INT_I32: case INT_U32: return "i32";
        default: return "i64";
        }
    case TY_FLOAT: return ty->data == 32 ? "float" : "double";
    case TY_STR: return "%astra.str";
    default: return "void";   // (), ! and the types the checker rejects
    }
}

bool llvm_has_value(TypeId type) {
    return type.id != TID_UNIT && type.id != TID_NEVER && type.id != TID_ERROR;
}

StrView def_name(Gen *g, DefId def) {
    return symbol_name(&g->sema->session->names, sema_def(g->sema, def)->name);
}

// User functions are prefixed so they never clash with C symbols; `main`
// becomes the entry point called by the runtime.
void function_name(Gen *g, Out *out, DefId def, bool is_main) {
    if (is_main) {
        out_str(out, "@astra_main");
        return;
    }
    const Def *d = sema_def(g->sema, def);
    out_str(out, "@\"astra.");
    bool method = d->item && d->item->kind != ITEM_FN && d->item->kind != ITEM_EXTERN;
    if (method) {   // a method: qualified by its type, `astra.Point.norm`
        StrView owner = symbol_name(&g->sema->session->names, d->item->name);
        out_write(out, owner.ptr, owner.len);
        out_char(out, '.');
    }
    StrView name = def_name(g, def);
    out_write(out, name.ptr, name.len);
    out_char(out, '"');
}

void place_address(Gen *g, LocalId local, FieldPath path, char text[16]) {
    if (path.len == 0) {
        fmt_into(text, 16, "%%l%u", local.id);
        return;
    }
    next_temp(g, text);
    out_fmt(&g->out, "  %s = getelementptr inbounds %s, ptr %%l%u, i32 0", text,
            llvm_type(g, g->body->locals.data[local.id].type), local.id);
    for (uint32_t i = 0; i < path.len; i++) {
        out_fmt(&g->out, ", i32 %u", path.fields[i]);
    }
    out_char(&g->out, '\n');
}

// "%t<N>", written by hand: this is the most frequent formatting.
void next_temp(Gen *g, char text[16]) {
    char digits[12];
    int n = 0;
    uint32_t value = g->temp++;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    text[0] = '%';
    text[1] = 't';
    for (int i = 0; i < n; i++) {
        text[2 + i] = digits[n - 1 - i];
    }
    text[2 + n] = '\0';
}

static void escaped_bytes(Out *out, StrView text) {
    for (size_t i = 0; i < text.len; i++) {
        unsigned char c = (unsigned char)text.ptr[i];
        if (c < 0x20 || c >= 0x7f || c == '"' || c == '\\') {
            out_fmt(out, "\\%02X", c);
        } else {
            out_char(out, (char)c);
        }
    }
}

uint32_t string_global(Gen *g, StrView text) {
    uint32_t n = g->str_count++;
    out_fmt(&g->globals, "@.str.%u = private unnamed_addr constant [%zu x i8] c\"", n, text.len);
    escaped_bytes(&g->globals, text);
    out_str(&g->globals, "\"\n");
    return n;
}

uint32_t location_global(Gen *g, Span span) {
    LineCol pos = source_line_col(&g->sema->session->sources, span.file, span.start);
    uint32_t n = g->loc_count++;
    out_fmt(&g->globals,
            "@.loc.%u = private unnamed_addr constant %%astra.loc { ptr @.file, i32 %u, i32 %u }\n",
            n, pos.line, pos.column);
    return n;
}

static const char RUNTIME_DECLS[] =
    "declare void @astra_print_str(ptr) nounwind\n"
    "declare void @astra_print_int(i64) nounwind\n"
    "declare void @astra_print_uint(i64) nounwind\n"
    "declare void @astra_print_float(double) nounwind\n"
    "declare void @astra_print_f32(float) nounwind\n"
    "declare void @astra_print_bool(i1 zeroext) nounwind\n"
    "declare void @astra_print_newline() nounwind\n"
    "declare void @astra_str_push_str(ptr, ptr) nounwind\n"
    "declare void @astra_str_push_int(ptr, i64) nounwind\n"
    "declare void @astra_str_push_uint(ptr, i64) nounwind\n"
    "declare void @astra_str_push_float(ptr, double) nounwind\n"
    "declare void @astra_str_push_f32(ptr, float) nounwind\n"
    "declare void @astra_str_push_bool(ptr, i1 zeroext) nounwind\n"
    "declare void @astra_str_concat(ptr, ptr, ptr) nounwind\n"
    "declare zeroext i1 @astra_str_eq(ptr, ptr) nounwind\n"
    "declare i32 @astra_str_cmp(ptr, ptr) nounwind\n"
    "declare void @astra_panic_overflow(ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_div_zero(ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_shift(ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_cast(ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_bounds(i64, i64, ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_assert(ptr) noreturn cold nounwind\n"
    "declare void @astra_panic_str(ptr, ptr) noreturn cold nounwind\n"
    "declare void @astra_str_clear(ptr) nounwind\n"
    "declare zeroext i1 @astra_str_starts_with(ptr, ptr) nounwind\n"
    "declare zeroext i1 @astra_str_ends_with(ptr, ptr) nounwind\n"
    "declare zeroext i1 @astra_str_contains(ptr, ptr) nounwind\n"
    "declare void @astra_str_repeat(ptr, ptr, i64) nounwind\n"
    "declare void @astra_str_to_lower(ptr, ptr) nounwind\n"
    "declare void @astra_str_to_upper(ptr, ptr) nounwind\n"
    "declare void @astra_str_to_owned(ptr, ptr) nounwind\n";

static void overflow_decls(Gen *g) {
    static const char *const OPS[] = {"add", "sub", "mul"};
    static const unsigned BITS[] = {8, 16, 32, 64};
    for (int i = 0; i < 2 * 3 * 4; i++) {
        int s = i / 12, op = i / 4 % 3, b = i % 4;
        if (g->overflow_used[s][op][b]) {
            out_fmt(&g->out, "declare { i%u, i1 } @llvm.%c%s.with.overflow.i%u(i%u, i%u)\n",
                    BITS[b], s ? 's' : 'u', OPS[op], BITS[b], BITS[b], BITS[b]);
        }
    }
}

Gen *llvm_begin(Sema *sema, FILE *file) {
    Gen *g = xcalloc(1, sizeof(Gen));
    g->sema = sema;
    g->types = &sema->types;
    out_to_file(&g->out, file);
    out_to_memory(&g->globals);
    out_to_memory(&g->type_defs);
    const SourceFile *source = source_file(&sema->session->sources, sema->module->file);
    out_to_memory(&g->path);
    escaped_bytes(&g->path, source->path);
    out_char(&g->path, '\0');
    out_fmt(&g->out, "; Generated by the Astra stage 0 compiler.\nsource_filename = \"%s\"\n\n",
            g->path.buf);
    out_str(&g->out, "%astra.str = type { ptr, i64, i64 }\n");
    out_str(&g->out, "%astra.loc = type { ptr, i32, i32 }\n");
    // Every struct is known after type checking. The definitions come first:
    // LLVM needs a type's size when a function allocates it.
    for (size_t i = 1; i < sema->defs.len; i++) {
        if (sema->defs.data[i].kind == DEF_STRUCT && sema->defs.data[i].fields) {
            struct_type(g, (uint32_t)i);
        }
    }
    out_write(&g->out, g->type_defs.buf, g->type_defs.len);
    out_char(&g->out, '\n');
    g->type_defs.len = 0;
    return g;
}

void llvm_function(Gen *g, const AirBody *body) {
    gen_body(g, body);
}

void llvm_end(Gen *g) {
    const SourceFile *source = source_file(&g->sema->session->sources, g->sema->module->file);
    out_str(&g->out, RUNTIME_DECLS);
    overflow_decls(g);
    gen_float_decls(g);
    out_fmt(&g->out, "\n@.file = private unnamed_addr constant [%zu x i8] c\"%s\\00\"\n",
            source->path.len + 1, g->path.buf);
    out_write(&g->out, g->globals.buf, g->globals.len);
    out_free(&g->type_defs);
    free(g->adt_types);
    out_free(&g->path);
    out_free(&g->globals);
    out_free(&g->out);
    free(g);
}
