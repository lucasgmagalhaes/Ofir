// LLVM types of structs, tuples and enums, their layout, and the addresses
// of places.
//
// An enum is `{ i32 tag, [K x i64] payload }`: the payload is large enough
// for the largest variant, each variant being a struct of its fields
// (`%"Shape.3.Circle"`) laid over the payload.

#include "codegen/llvm_internal.h"

#include <stdio.h>
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

// The field types of variant `v` of the enum instance `adt` (or of the
// struct when `v` is -1), in an arena array.
static const TypeId *field_types(Gen *g, TypeId adt, int64_t v, uint32_t count) {
    TypeId *types = ARENA_ARRAY(&g->sema->session->arena, TypeId, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        types[i] = v < 0 ? struct_field_type(g->sema, adt, i)
                         : variant_field_type(g->sema, adt, (uint32_t)v, i);
    }
    return types;
}

// The payload words of an enum: enough for its largest variant.
static uint64_t payload_words(Gen *g, TypeId adt, const EnumInfo *info) {
    uint64_t largest = 0;
    for (uint32_t v = 0; v < info->count; v++) {
        uint32_t count = info->variants[v].count;
        Layout l = fields_layout(g, field_types(g, adt, v, count), count);
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
            uint32_t count = def->fields->count;
            return fields_layout(g, field_types(g, type, -1, count), count);
        }
        uint64_t words = payload_words(g, type, def->variants);
        return words ? (Layout){8 + 8 * words, 8} : (Layout){4, 4};
    }
    default: return (Layout){0, 1};
    }
}

static const char *field_type(Gen *g, TypeId type) {
    const char *t = llvm_type(g, type);
    return strcmp(t, "void") ? t : "{}";
}

// An ADT in a type name, qualified by its module outside the root module,
// so that the names of instances are unique: `Pair<int, geo.Point>`.
static StrView qualified_name(const void *context, const Ty *ty) {
    static char buf[256];
    const Sema *sema = context;
    StrView name = sema_type_name(context, ty);
    if (ty->kind != TY_ADT || sema->defs.data[ty->data].module == 0) {
        return name;
    }
    const Def *def = &sema->defs.data[ty->data];
    StrView module = sema->program.items[def->module].name;
    int len = snprintf(buf, sizeof buf, SV_FMT "." SV_FMT, SV_ARG(module), SV_ARG(name));
    return (StrView){buf, len < (int)sizeof buf ? (size_t)len : sizeof buf - 1};
}

void type_args_text(Gen *g, Out *out, TypeId targs) {
    const Ty *ty = type_get(g->types, targs);
    StrBuf text = {0};
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        strbuf_append_cstr(&text, i ? ", " : "<");
        ty_text(&text, g->types, ty->args[i], qualified_name, g->sema);
    }
    strbuf_push(&text, '>');
    out_write(out, text.data, text.len);
    strbuf_free(&text);
}

// `%"Shape.3"`, or `%"Pair<int, str>.5"` for an instance: the DefId keeps
// same-named types apart.
static char *name_type(Gen *g, TypeId type, const char *suffix) {
    const Ty *ty = type_get(g->types, type);
    StrView name = def_name(g, (DefId){ty->data});
    Out out;
    out_to_memory(&out);
    out_str(&out, "%\"");
    out_write(&out, name.ptr, name.len);
    if (ty->arg_count > 0) {
        type_args_text(g, &out, type_tuple(g->types, ty->args, ty->arg_count));
    }
    out_fmt(&out, ".%u%s\"", type_get(g->types, type)->data, suffix);
    char *text = ARENA_ARRAY(&g->sema->session->arena, char, out.len + 1);
    memcpy(text, out.buf, out.len);
    text[out.len] = '\0';
    out_free(&out);
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

// The LLVM name of each ADT type, by TypeId; the table grows with the
// type table, which instances extend.
static const char **slot(Gen *g, TypeId type) {
    if (type.id >= g->adt_count) {
        size_t count = g->types->count > type.id ? g->types->count : type.id + 1;
        g->adt_types = xrealloc(g->adt_types, count * sizeof(char *));
        for (size_t i = g->adt_count; i < count; i++) {
            g->adt_types[i] = NULL;
        }
        g->adt_count = count;
    }
    return &g->adt_types[type.id];
}

// The LLVM type of the variant `v` of an enum.
const char *variant_type(Gen *g, TypeId adt, uint32_t v) {
    char suffix[16];
    fmt_into(suffix, sizeof suffix, ".%u", v);
    return name_type(g, adt, suffix);
}

static const char *define_enum(Gen *g, TypeId adt, const char *name) {
    const EnumInfo *info = sema_def(g->sema, (DefId){type_get(g->types, adt)->data})->variants;
    uint64_t words = payload_words(g, adt, info);
    for (uint32_t v = 0; v < info->count; v++) {
        uint32_t count = info->variants[v].count;
        define(g, variant_type(g, adt, v), field_types(g, adt, v, count), count);
    }
    if (words) {
        out_fmt(&g->type_defs, "%s = type { i32, [%llu x i64] }\n", name,
                (unsigned long long)words);
    } else {
        out_fmt(&g->type_defs, "%s = type { i32 }\n", name);
    }
    return name;
}

const char *adt_type(Gen *g, TypeId type) {
    const char **known = slot(g, type);
    if (*known) {
        return *known;
    }
    const Def *d = sema_def(g->sema, (DefId){type_get(g->types, type)->data});
    char *name = name_type(g, type, "");
    *slot(g, type) = name;   // name_type may have grown the table
    if (d->kind == DEF_ENUM) {
        return define_enum(g, type, name);
    }
    uint32_t count = d->fields ? d->fields->count : 0;
    define(g, name, field_types(g, type, -1, count), count);
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
    TypeId type;              // the current value; the enum when `variant` is set
    bool variant;
    uint32_t index;           // the variant, when `variant` is set
} Cursor;

static void step(Gen *g, Cursor *cur, uint32_t entry, char base[16]) {
    char next[16];
    next_temp(g, next);
    const Ty *ty = type_get(g->types, cur->type);
    if (entry & PATH_VARIANT) {   // view the payload as a variant
        out_fmt(&g->out, "  %s = getelementptr inbounds %s, ptr %s, i32 0, i32 1\n", next,
                llvm_type(g, cur->type), base);
        cur->variant = true;
        cur->index = entry & ~PATH_VARIANT;
        memcpy(base, next, 16);
        return;
    }
    const char *container = cur->variant ? variant_type(g, cur->type, cur->index)
                                         : llvm_type(g, cur->type);
    out_fmt(&g->out, "  %s = getelementptr inbounds %s, ptr %s, i32 0, i32 %u\n", next,
            container, base, entry);
    ty = type_get(g->types, cur->type);   // llvm_type may have grown the type table
    bool is_enum = ty->kind == TY_ADT && sema_def(g->sema, (DefId){ty->data})->kind == DEF_ENUM;
    if (cur->variant) {
        cur->type = variant_field_type(g->sema, cur->type, cur->index, entry);
    } else if (ty->kind == TY_TUPLE) {
        cur->type = ty->args[entry];
    } else {   // a struct, or a primitive's fields from its `@lang` declaration
        cur->type = is_enum ? type_id(TID_I32) : struct_field_type(g->sema, cur->type, entry);
    }
    cur->variant = false;
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
