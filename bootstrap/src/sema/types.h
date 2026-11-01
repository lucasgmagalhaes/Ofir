// Types: hash-consed, so each distinct type exists once and type equality is
// a comparison of `TypeId`s (docs/design/compiler-architecture.md, 2).

#ifndef ASTRA_TYPES_H
#define ASTRA_TYPES_H

#include "base/arena.h"
#include "base/intern.h"
#include "base/str.h"
#include "sema/ids.h"

typedef enum {
    TY_ERROR,    // after a reported error; compatible with every type
    TY_UNIT,     // ()
    TY_NEVER,    // !
    TY_BOOL,
    TY_INT,      // i8 ... u64; `int` and `uint` are aliases
    TY_FLOAT,    // f32, f64; `float` is an alias
    TY_STR,
    TY_REF,      // &T, &mut T
    TY_PTR,      // *T, *mut T
    TY_SLICE,    // [T]
    TY_ARRAY,    // [T; N]
    TY_TUPLE,    // (A, B)
    TY_FN,       // fn(A, B) R
    TY_ADT,      // a struct or an enum, with type arguments
    TY_PARAM,    // a type parameter of the enclosing declaration
} TyKind;

typedef enum { INT_I8, INT_I16, INT_I32, INT_I64, INT_U8, INT_U16, INT_U32, INT_U64 } IntKind;

typedef struct {
    TyKind kind;
    uint32_t data;      // int: IntKind; float: bits; ref, ptr: mutable; array: length;
                        // adt: DefId; param: index
    TypeId inner;       // ref, ptr, slice, array: element; fn: return type;
                        // param: its name (a Symbol id), for messages
    uint32_t arg_count; // tuple: elements; fn: parameters; adt: type arguments
    const TypeId *args;
    bool has_param;     // set by type_intern: a TY_PARAM occurs in the type
} Ty;

// The types created by `types_init`, in this order.
enum {
    TID_ERROR, TID_UNIT, TID_NEVER, TID_BOOL,
    TID_I8, TID_I16, TID_I32, TID_I64, TID_U8, TID_U16, TID_U32, TID_U64,
    TID_F32, TID_F64, TID_STR,
    TID_BUILTIN_COUNT,
};

#define TID_INT TID_I64     // `int`
#define TID_FLOAT TID_F64   // `float`

typedef struct {
    Arena *arena;          // owns the argument arrays
    Ty *types;             // types[id]
    uint32_t count, cap;
    uint32_t *slots;       // open addressing table of id + 1 (0 = empty)
    uint32_t slots_cap;    // a power of two
} TypeTable;

void types_init(TypeTable *table, Arena *arena);
void types_free(TypeTable *table);

// Returns the id of `ty`, creating it the first time. `ty.args` is copied.
TypeId type_intern(TypeTable *table, Ty ty);

static inline const Ty *type_get(const TypeTable *table, TypeId id) {
    return &table->types[id.id];
}

static inline TypeId type_id(uint32_t id) {
    return (TypeId){id};
}

static inline bool type_eq(TypeId a, TypeId b) {
    return a.id == b.id;
}

TypeId type_ref(TypeTable *table, TypeId inner, bool mutable);
TypeId type_slice(TypeTable *table, TypeId elem);
TypeId type_tuple(TypeTable *table, const TypeId *elems, uint32_t count);
TypeId type_fn(TypeTable *table, const TypeId *params, uint32_t count, TypeId ret);

// The type parameter number `index` of a generic declaration. The type
// parameters of a type come first, then those of its method.
TypeId type_param(TypeTable *table, uint32_t index, Symbol name);

// `type` with each TY_PARAM `i` replaced by `args[i]` (spec 8.6).
TypeId type_subst(TypeTable *table, TypeId type, const TypeId *args, uint32_t count);

// How deeply types nest in `type`: 1 for `int`, 2 for `Pair<int, int>`.
uint32_t type_depth(const TypeTable *table, TypeId type);

bool type_is_integer(const TypeTable *table, TypeId id);
bool type_is_numeric(const TypeTable *table, TypeId id);

// Appends the source spelling of a type, as in `&mut List<int>`. The names
// of ADTs and type parameters come from `name_of`, which may be NULL in tests.
typedef StrView (*TypeNameFn)(const void *context, const Ty *ty);
void ty_text(StrBuf *out, const TypeTable *table, TypeId id, TypeNameFn name_of,
             const void *context);

#endif
