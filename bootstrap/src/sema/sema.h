// Semantic analysis: name resolution (resolve*.c) and type checking
// (check*.c) of one module. Results are stored as annotations in the AST.

#ifndef ASTRA_SEMA_H
#define ASTRA_SEMA_H

#include "sema/scope.h"
#include "sema/types.h"
#include "session.h"
#include "syntax/ast.h"

typedef enum {
    DEF_FN,
    DEF_BUILTIN,     // print, println, panic, assert
    DEF_PRIMITIVE,   // int, str, ...: `type` holds the type
    DEF_STRUCT,
    DEF_ENUM,
    DEF_CONST,
    DEF_GLOBAL,
    DEF_TYPE_ALIAS,
} DefKind;

typedef enum { BUILTIN_PRINT, BUILTIN_PRINTLN, BUILTIN_PANIC, BUILTIN_ASSERT } Builtin;

// The fields of a struct, computed by the checker (sema/structs.c).
typedef struct {
    uint32_t count;
    Symbol *names;       // 0 for the fields of a tuple struct
    TypeId *types;
    Expr **defaults;     // NULL when the field has no default
    bool is_tuple, is_copy;
} StructInfo;

// The variants of an enum, computed by the checker (sema/enums.c).
typedef struct {
    Symbol name;
    uint32_t count;      // payload fields
    Symbol *field_names; // 0 for an unnamed field
    TypeId *types;
    int64_t discriminant;
} VariantInfo;

typedef struct {
    uint32_t count;
    VariantInfo *variants;
    bool has_payload;    // some variant has fields
} EnumInfo;

typedef struct {
    DefKind kind;
    Symbol name;
    Span span;           // empty for the prelude
    const Item *item;    // NULL for the prelude and functions in extern blocks
    FnDecl *fn;          // functions
    TypeId type;         // primitive: the type; function: its signature (set by the checker)
    Builtin builtin;
    bool in_prelude;
    bool typed;          // `type` is set (signatures, constants)
    bool checking;       // a constant whose type is being computed (cycle guard)
    const struct ConstValue *value;   // a constant's value, NULL if it could not be computed
    StructInfo *fields;  // structs, once computed
    EnumInfo *variants;  // enums, once computed
} Def;

typedef struct {
    Session *session;
    Module *module;
    TypeTable types;
    Vec(Def) defs;       // defs.data[0] is unused: DefId 0 means "none"
    Scope scope;
} Sema;

void sema_init(Sema *sema, Session *session, Module *module);
void sema_free(Sema *sema);

static inline Def *sema_def(Sema *sema, DefId id) {
    return &sema->defs.data[id.id];
}

// Binds every name in the module (E0200, E0201).
void resolve_module(Sema *sema);

// Type checks the module after resolve_module.
void check_module(Sema *sema);

#endif
