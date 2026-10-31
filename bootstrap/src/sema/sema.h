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
