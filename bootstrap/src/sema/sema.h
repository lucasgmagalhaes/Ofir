// Semantic analysis: name resolution (resolve*.c) and type checking
// (check*.c) of one module. Results are stored as annotations in the AST.

#ifndef ASTRA_SEMA_H
#define ASTRA_SEMA_H

#include "sema/scope.h"
#include "sema/types.h"
#include "session.h"
#include "syntax/ast.h"

#include <stdio.h>

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
    DEF_VARIANT,     // `Some`, `None`, `Ok`, `Err`: variant `variant` of the enum `target`
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
    uint32_t target;     // DEF_MODULE: the imported module; DEF_VARIANT: the enum's DefId
    uint32_t variant;    // DEF_VARIANT: the variant's position
    bool typed;          // `type` is set (signatures, constants)
    bool checking;       // a constant whose type is being computed (cycle guard)
    const struct ConstValue *value;   // a constant's value, NULL if it could not be computed
    StructInfo *fields;  // structs, once computed
    EnumInfo *variants;  // enums, once computed
} Def;

// An import, for the unused-import warning (W0002).
typedef struct {
    uint32_t module;     // the importing module
    Symbol name;         // the name it binds
    Span span;
    bool used;
} ImportUse;

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
    Vec(ImportUse) imports;   // in module order
    DefId lang[TID_BUILTIN_COUNT];   // the `@lang` declaration of each primitive type
    DefId box;           // `@lang("box") struct Box<T>`, which the compiler dereferences
    DefId list;          // `@lang("list") struct List<T>`: literals, indexing, `for`
    DefId option, result;   // `@lang("option")`, `@lang("result")`: `?`, `??`, bare variants
    DefId slice;         // `@lang("slice") ref struct Slice<T>`: the type `&[T]`
    DefId map;           // `@lang("map") struct Map<K, V>`: `for key, value in map`
    bool no_moves;       // --emit=types: no move analysis (stage 1 ports it later)
    FILE *trace;         // --emit=names: where the root module's resolutions go
    bool testing;        // `astra test`: the `@test` functions are the roots, not `main`
    Vec(FnDecl *) tests; // filled by the lowering when testing, in program order
} Sema;

void sema_init(Sema *sema, Session *session, Program program);
void sema_free(Sema *sema);

// The attribute `@name(...)` of a declaration, or NULL.
const Attribute *find_attribute(const Sema *sema, AttributeList attributes, const char *name);

// A `@test` function that `astra test` does not run (all of them in other
// builds, and those of std modules other than the root): not resolved,
// checked or compiled, so that tests cost nothing elsewhere (spec 12.2).
bool skipped_test(const Sema *sema, const Item *item);

// Whether module `module` belongs to the standard library (`std.*`).
bool module_in_std(const Sema *sema, uint32_t module);

// The `@lang` declaration of a primitive type, or NULL.
Def *lang_def(Sema *sema, TypeId type);

// The type a method's `self` and `Self` stand for: the struct or enum, or
// the primitive of a `@lang` declaration. For a generic type it is the type
// applied to its own parameters, `Stack<T>`.
TypeId owner_type(Sema *sema, const Item *owner);

// The type parameters of a struct or enum (none for other items).
SymbolList item_type_params(const Item *item);

// How many type arguments a call of the function `def` takes: those of its
// type, for a method, then its own (spec 8.3).
uint32_t fn_type_param_count(const Def *def);

// The type of field `i` of a struct, or of field `i` of variant `v` of an
// enum, in the instance `adt` (`Pair<int, str>` gives `int` for `T`).
TypeId struct_field_type(Sema *sema, TypeId adt, uint32_t i);
TypeId variant_field_type(Sema *sema, TypeId adt, uint32_t v, uint32_t i);

// The name of an ADT or a type parameter, for ty_text; `context` is the Sema.
StrView sema_type_name(const void *context, const Ty *ty);

// Makes module `index` current; returns the previous one.
uint32_t sema_enter(Sema *sema, uint32_t index);

static inline Def *sema_def(Sema *sema, DefId id) {
    return &sema->defs.data[id.id];
}

// Records that the current module names `name`, which may be an import.
void note_import_use(Sema *sema, Symbol name);
// W0002 for each import nothing names, except in the prelude's root.
void warn_unused_imports(Sema *sema);

// check_trace.c: --emit=types
void trace_types(Sema *sema, FILE *out);

// Binds every name in the program (E0200, E0201, E0202, E0204).
void resolve_program(Sema *sema);

// Type checks the program after resolve_program.
void check_program(Sema *sema);

// Runs the move analysis of a std function before it is lowered: std
// functions are analyzed only if the program uses them (sema/check.c).
void sema_moves_late(Sema *sema, FnDecl *fn);

// Checks the body of a std function the lowering reached, which the checker
// left for later (only the root module and the modules outside std are
// checked whole).
void sema_check_late(Sema *sema, FnDecl *fn);

#endif
