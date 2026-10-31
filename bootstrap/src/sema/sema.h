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
    DEF_MODULE,      // an imported module: `target` is its index in the program
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
    bool is_pub;
    bool lang;           // a `@lang` declaration: `type` is the primitive it describes
    uint32_t module;     // the module that declares it (index in the program)
    uint32_t target;     // DEF_MODULE: the imported module
    bool typed;          // `type` is set (signatures, constants)
    bool checking;       // a constant whose type is being computed (cycle guard)
    const struct ConstValue *value;   // a constant's value, NULL if it could not be computed
    StructInfo *fields;  // structs, once computed
    EnumInfo *variants;  // enums, once computed
} Def;

// The modules of a program; the root module (with `main`) comes first.
typedef struct {
    Module *module;
    StrView name;        // "parser.lexer"; the root's is its file name without `.at`
} ProgramModule;

typedef struct {
    ProgramModule *items;
    uint32_t len;
} Program;

typedef struct {
    Session *session;
    Program program;
    uint32_t current;    // the module being resolved, checked or lowered
    Module *module;      // program.items[current].module
    TypeTable types;
    Vec(Def) defs;       // defs.data[0] is unused: DefId 0 means "none"
    Scope scope;         // its module table is the current module's
    DefId lang[TID_BUILTIN_COUNT];   // the `@lang` declaration of each primitive type
} Sema;

void sema_init(Sema *sema, Session *session, Program program);
void sema_free(Sema *sema);

// The attribute `@name(...)` of a declaration, or NULL.
const Attribute *find_attribute(const Sema *sema, AttributeList attributes, const char *name);

// The `@lang` declaration of a primitive type, or NULL.
Def *lang_def(Sema *sema, TypeId type);

// The type a method's `self` and `Self` stand for: the struct or enum, or
// the primitive of a `@lang` declaration.
TypeId owner_type(Sema *sema, const Item *owner);

// Makes module `index` current; returns the previous one.
uint32_t sema_enter(Sema *sema, uint32_t index);

static inline Def *sema_def(Sema *sema, DefId id) {
    return &sema->defs.data[id.id];
}

// Binds every name in the program (E0200, E0201, E0202, E0204).
void resolve_program(Sema *sema);

// Type checks the program after resolve_program.
void check_program(Sema *sema);

#endif
