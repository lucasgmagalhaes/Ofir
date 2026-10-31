#include "sema/types.h"

#include <stdlib.h>
#include <string.h>

static uint64_t mix(uint64_t h, uint64_t value) {
    h ^= value + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

static uint64_t ty_hash(const Ty *ty) {
    uint64_t h = mix(ty->kind, ty->data);
    h = mix(h, ty->inner.id);
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        h = mix(h, ty->args[i].id);
    }
    return h;
}

static bool ty_equal(const Ty *a, const Ty *b) {
    if (a->kind != b->kind || a->data != b->data || a->inner.id != b->inner.id ||
        a->arg_count != b->arg_count) {
        return false;
    }
    return a->arg_count == 0 || memcmp(a->args, b->args, a->arg_count * sizeof(TypeId)) == 0;
}

static void rehash(TypeTable *table) {
    uint32_t cap = table->slots_cap ? table->slots_cap * 2 : 256;
    uint32_t *slots = xcalloc(cap, sizeof(uint32_t));
    for (uint32_t id = 0; id < table->count; id++) {
        uint32_t i = (uint32_t)ty_hash(&table->types[id]) & (cap - 1);
        while (slots[i] != 0) {
            i = (i + 1) & (cap - 1);
        }
        slots[i] = id + 1;
    }
    free(table->slots);
    table->slots = slots;
    table->slots_cap = cap;
}

static TypeId push(TypeTable *table, Ty ty, uint32_t slot) {
    if (ty.arg_count > 0) {
        TypeId *args = ARENA_ARRAY(table->arena, TypeId, ty.arg_count);
        memcpy(args, ty.args, ty.arg_count * sizeof(TypeId));
        ty.args = args;
    }
    if (table->count == table->cap) {
        table->cap = table->cap ? table->cap * 2 : 64;
        table->types = xrealloc(table->types, table->cap * sizeof(Ty));
    }
    table->slots[slot] = table->count + 1;
    table->types[table->count] = ty;
    return type_id(table->count++);
}

TypeId type_intern(TypeTable *table, Ty ty) {
    if ((table->count + 1) * 2 > table->slots_cap) {
        rehash(table);   // keep the load factor under 1/2
    }
    uint32_t mask = table->slots_cap - 1;
    uint32_t i = (uint32_t)ty_hash(&ty) & mask;
    for (; table->slots[i] != 0; i = (i + 1) & mask) {
        uint32_t id = table->slots[i] - 1;
        if (ty_equal(&table->types[id], &ty)) {
            return type_id(id);
        }
    }
    return push(table, ty, i);
}

void types_init(TypeTable *table, Arena *arena) {
    *table = (TypeTable){.arena = arena};
    // Same order as the TYPE_* constants.
    type_intern(table, (Ty){.kind = TY_ERROR});
    type_intern(table, (Ty){.kind = TY_UNIT});
    type_intern(table, (Ty){.kind = TY_NEVER});
    type_intern(table, (Ty){.kind = TY_BOOL});
    for (uint32_t k = INT_I8; k <= INT_U64; k++) {
        type_intern(table, (Ty){.kind = TY_INT, .data = k});
    }
    type_intern(table, (Ty){.kind = TY_FLOAT, .data = 32});
    type_intern(table, (Ty){.kind = TY_FLOAT, .data = 64});
    type_intern(table, (Ty){.kind = TY_STR});
    if (table->count != TYPE_BUILTIN_COUNT) {
        fatal("types_init: built-in types out of order");
    }
}

void types_free(TypeTable *table) {
    free(table->types);
    free(table->slots);
    *table = (TypeTable){0};
}

TypeId type_ref(TypeTable *table, TypeId inner, bool mutable) {
    return type_intern(table, (Ty){.kind = TY_REF, .data = mutable, .inner = inner});
}

TypeId type_slice(TypeTable *table, TypeId elem) {
    return type_intern(table, (Ty){.kind = TY_SLICE, .inner = elem});
}

TypeId type_tuple(TypeTable *table, const TypeId *elems, uint32_t count) {
    if (count == 0) {
        return type_id(TYPE_UNIT);
    }
    return type_intern(table, (Ty){.kind = TY_TUPLE, .arg_count = count, .args = elems});
}

TypeId type_fn(TypeTable *table, const TypeId *params, uint32_t count, TypeId ret) {
    return type_intern(table,
                       (Ty){.kind = TY_FN, .inner = ret, .arg_count = count, .args = params});
}

bool type_is_integer(const TypeTable *table, TypeId id) {
    return type_get(table, id)->kind == TY_INT;
}

bool type_is_numeric(const TypeTable *table, TypeId id) {
    TyKind kind = type_get(table, id)->kind;
    return kind == TY_INT || kind == TY_FLOAT;
}

// The 64-bit types are printed with their usual alias names.
static const char *const INT_NAMES[] = {"i8", "i16", "i32", "int", "u8", "u16", "u32", "uint"};

static void list_text(StrBuf *out, const TypeTable *table, const Ty *ty, AdtNameFn adt_name,
                      const void *context) {
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        if (i > 0) {
            strbuf_append_cstr(out, ", ");
        }
        ty_text(out, table, ty->args[i], adt_name, context);
    }
}

// check-sizes: table
void ty_text(StrBuf *out, const TypeTable *table, TypeId id, AdtNameFn adt_name,
             const void *context) {
    const Ty *ty = type_get(table, id);
    switch (ty->kind) {
    case TY_ERROR: strbuf_append_cstr(out, "{error}"); break;
    case TY_UNIT: strbuf_append_cstr(out, "()"); break;
    case TY_NEVER: strbuf_push(out, '!'); break;
    case TY_BOOL: strbuf_append_cstr(out, "bool"); break;
    case TY_INT: strbuf_append_cstr(out, INT_NAMES[ty->data]); break;
    case TY_FLOAT: strbuf_append_cstr(out, ty->data == 64 ? "float" : "f32"); break;
    case TY_STR: strbuf_append_cstr(out, "str"); break;
    case TY_REF:
    case TY_PTR:
        strbuf_push(out, ty->kind == TY_REF ? '&' : '*');
        strbuf_append_cstr(out, ty->data ? "mut " : "");
        ty_text(out, table, ty->inner, adt_name, context);
        break;
    case TY_SLICE:
    case TY_ARRAY:
        strbuf_push(out, '[');
        ty_text(out, table, ty->inner, adt_name, context);
        if (ty->kind == TY_ARRAY) {
            strbuf_appendf(out, "; %u", ty->data);
        }
        strbuf_push(out, ']');
        break;
    case TY_TUPLE:
        strbuf_push(out, '(');
        list_text(out, table, ty, adt_name, context);
        strbuf_append_cstr(out, ty->arg_count == 1 ? ",)" : ")");
        break;
    case TY_FN:
        strbuf_append_cstr(out, "fn(");
        list_text(out, table, ty, adt_name, context);
        strbuf_push(out, ')');
        if (!type_eq(ty->inner, type_id(TYPE_UNIT))) {
            strbuf_push(out, ' ');
            ty_text(out, table, ty->inner, adt_name, context);
        }
        break;
    case TY_ADT:
        if (adt_name) {
            strbuf_append(out, adt_name(context, ty->data));
        } else {
            strbuf_appendf(out, "adt%u", ty->data);
        }
        if (ty->arg_count > 0) {
            strbuf_push(out, '<');
            list_text(out, table, ty, adt_name, context);
            strbuf_push(out, '>');
        }
        break;
    case TY_PARAM: strbuf_appendf(out, "T%u", ty->data); break;
    }
}
