// LLVM types of structs, tuples and enums, their layout, and the addresses
// of places.
//
// An enum is `{ i32 tag, [K x i64] payload }`: the payload is large enough
// for the largest variant, each variant being a struct of its fields
// (`%"Shape.3.Circle"`) laid over the payload.

#include "codegen/llvm_internal.h"

#include <string.h>

typedef struct {
    uint64_t size, align;
} Layout;

static Layout layout(Gen *g, TypeId type);

static Layout fields_layout(Gen *g, const TypeId *types, uint32_t count) {
    Layout l = {0, 1};
    for (uint32_t i = 0; i < count; i++) {
        Layout f = layout(g, types[i]);
        l.size = (l.size + f.align - 1) / f.align * f.align + f.size;
        l.align = f.align > l.align ? f.align : l.align;
    }
    l.size = (l.size + l.align - 1) / l.align * l.align;
    return l;
}

// The payload words of an enum: enough for its largest variant.
static uint64_t payload_words(Gen *g, const EnumInfo *info) {
    uint64_t largest = 0;
    for (uint32_t v = 0; v < info->count; v++) {
        Layout l = fields_layout(g, info->variants[v].types, info->variants[v].count);
        largest = l.size > largest ? l.size : largest;
    }
    return (largest + 7) / 8;
}

// check-sizes: table
static Layout layout(Gen *g, TypeId type) {
    static const uint64_t INT_BYTES[] = {1, 2, 4, 8, 1, 2, 4, 8};
    const Ty *ty = type_get(g->types, type);
    switch (ty->kind) {
    case TY_BOOL: return (Layout){1, 1};
    case TY_INT: return (Layout){INT_BYTES[ty->data], INT_BYTES[ty->data]};
    case TY_FLOAT: return (Layout){ty->data / 8, ty->data / 8};
    case TY_STR: return (Layout){24, 8};
    case TY_REF: case TY_PTR: return (Layout){8, 8};
    case TY_TUPLE: return fields_layout(g, ty->args, ty->arg_count);
    case TY_ADT: {
        const Def *def = sema_def(g->sema, (DefId){ty->data});
        if (def->kind == DEF_STRUCT) {
            return fields_layout(g, def->fields->types, def->fields->count);
        }
        uint64_t words = payload_words(g, def->variants);
        return words ? (Layout){8 + 8 * words, 8} : (Layout){4, 4};
    }
    default: return (Layout){0, 1};
    }
}

static const char *field_type(Gen *g, TypeId type) {
    const char *t = llvm_type(g, type);
    return strcmp(t, "void") ? t : "{}";
}

static char *name_type(Gen *g, uint32_t def, const char *suffix) {
    StrView name = def_name(g, (DefId){def});
    size_t size = name.len + strlen(suffix) + 24;
    char *text = ARENA_ARRAY(&g->sema->session->arena, char, size);
    Out out;
    out_to_array(&out, text, size);
    out_str(&out, "%\"");
    out_write(&out, name.ptr, name.len);
    out_fmt(&out, ".%u%s\"", def, suffix);   // the DefId keeps same-named types apart
    return text;
}

static void define(Gen *g, const char *name, const TypeId *types, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        field_type(g, types[i]);   // define the field types first: no interleaving
    }
    out_fmt(&g->type_defs, "%s = type { ", name);
    for (uint32_t i = 0; i < count; i++) {
        out_fmt(&g->type_defs, "%s%s", i ? ", " : "", field_type(g, types[i]));
    }
    out_str(&g->type_defs, " }\n");
}

static const char **slot(Gen *g, uint32_t def) {
    if (def >= g->adt_count) {
        size_t count = g->sema->defs.len;
        g->adt_types = xrealloc(g->adt_types, count * sizeof(char *));
        for (size_t i = g->adt_count; i < count; i++) {
            g->adt_types[i] = NULL;
        }
        g->adt_count = count;
    }
    return &g->adt_types[def];
}

// The LLVM type of the variant `v` of an enum.
const char *variant_type(Gen *g, uint32_t def, uint32_t v) {
    char suffix[16];
    fmt_into(suffix, sizeof suffix, ".%u", v);
    return name_type(g, def, suffix);
}

static const char *define_enum(Gen *g, uint32_t def, const char *name) {
    const EnumInfo *info = sema_def(g->sema, (DefId){def})->variants;
    uint64_t words = payload_words(g, info);
    for (uint32_t v = 0; v < info->count; v++) {
        define(g, variant_type(g, def, v), info->variants[v].types, info->variants[v].count);
    }
    if (words) {
        out_fmt(&g->type_defs, "%s = type { i32, [%llu x i64] }\n", name,
                (unsigned long long)words);
    } else {
        out_fmt(&g->type_defs, "%s = type { i32 }\n", name);
    }
    return name;
}

const char *adt_type(Gen *g, uint32_t def) {
    const char **known = slot(g, def);
    if (*known) {
        return *known;
    }
    const Def *d = sema_def(g->sema, (DefId){def});
    char *name = name_type(g, def, "");
    *known = name;
    if (d->kind == DEF_ENUM) {
        return define_enum(g, def, name);
    }
    define(g, name, d->fields ? d->fields->types : NULL, d->fields ? d->fields->count : 0);
    return name;
}

// A tuple is a literal LLVM struct type, `{ i64, double }`.
const char *tuple_type(Gen *g, const Ty *ty) {
    size_t size = 4;
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        size += strlen(field_type(g, ty->args[i])) + 2;
    }
    char *text = ARENA_ARRAY(&g->sema->session->arena, char, size);
    Out out;
    out_to_array(&out, text, size);
    out_str(&out, "{ ");
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        out_fmt(&out, "%s%s", i ? ", " : "", field_type(g, ty->args[i]));
    }
    out_str(&out, " }");
    return text;
}

// One step of a place: from a struct, tuple or variant to one of its fields.
typedef struct {
    TypeId type;              // the current value, unless `variant` is set
    const VariantInfo *variant;
    uint32_t def, index;      // the enum and the variant, when `variant` is set
} Cursor;

static void step(Gen *g, Cursor *cur, uint32_t entry, char base[16]) {
    char next[16];
    next_temp(g, next);
    const Ty *ty = type_get(g->types, cur->type);
    if (entry & PATH_VARIANT) {   // view the payload as a variant
        out_fmt(&g->out, "  %s = getelementptr inbounds %s, ptr %s, i32 0, i32 1\n", next,
                llvm_type(g, cur->type), base);
        cur->def = ty->data;
        cur->index = entry & ~PATH_VARIANT;
        cur->variant = &sema_def(g->sema, (DefId){ty->data})->variants->variants[cur->index];
        memcpy(base, next, 16);
        return;
    }
    const char *container = cur->variant ? variant_type(g, cur->def, cur->index)
                                         : llvm_type(g, cur->type);
    out_fmt(&g->out, "  %s = getelementptr inbounds %s, ptr %s, i32 0, i32 %u\n", next,
            container, base, entry);
    if (cur->variant) {
        cur->type = cur->variant->types[entry];
    } else if (ty->kind == TY_TUPLE) {
        cur->type = ty->args[entry];
    } else {
        const Def *def = sema_def(g->sema, (DefId){ty->data});
        cur->type = def->kind == DEF_STRUCT ? def->fields->types[entry] : type_id(TID_I32);
    }
    cur->variant = NULL;
    memcpy(base, next, 16);
}

void place_address(Gen *g, LocalId local, FieldPath path, char text[16]) {
    char base[16];
    fmt_into(base, sizeof base, "%%l%u", local.id);
    if (path.deref) {   // the local holds the address of the value
        next_temp(g, base);
        out_fmt(&g->out, "  %s = load ptr, ptr %%l%u\n", base, local.id);
    }
    Cursor cur = {.type = g->body->locals.data[local.id].type};
    const Ty *pointer = type_get(g->types, cur.type);
    if (path.deref && pointer->kind == TY_PTR) {   // `*p`: start from the pointee
        cur.type = pointer->inner;
    }
    for (uint32_t i = 0; i < path.len; i++) {
        step(g, &cur, path.fields[i], base);
    }
    memcpy(text, base, 16);
}
