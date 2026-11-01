// Abstract syntax tree (spec appendix A). Nodes live in the session arena.
//
// The AST is a faithful representation of the source: no desugaring happens
// in the parser.

#ifndef ASTRA_AST_H
#define ASTRA_AST_H

#include "base/intern.h"
#include "base/source.h"
#include "sema/ids.h"

#include <stdio.h>

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct TypeExpr TypeExpr;
typedef struct Pattern Pattern;
typedef struct Block Block;
typedef struct FnDecl FnDecl;

// An immutable array allocated in the arena.
#define List(T) struct { T *items; uint32_t len; }

typedef List(Expr *) ExprList;
typedef List(Stmt *) StmtList;
typedef List(TypeExpr *) TypeList;
typedef List(Pattern *) PatternList;
typedef List(Symbol) SymbolList;
typedef List(FnDecl *) FnList;

// ---------------------------------------------------------------------------
// Semantic annotations: zero after parsing, filled in by sema/.

typedef enum {
    RES_NONE,       // not resolved yet
    RES_LOCAL,      // index: a LocalId of the enclosing function
    RES_DEF,        // index: a DefId
    RES_BUILTIN,    // index: a BuiltinMethod (sema/builtins.h)
    RES_FIELD,      // index: the position of a struct field
    RES_VARIANT,    // index: the position of an enum variant
    RES_ERROR,      // an error was reported
} ResKind;

typedef struct {
    ResKind kind;
    uint32_t index;
} Res;

// A local variable or parameter of one function body.
typedef struct {
    Symbol name;
    Span span;
    bool mutable;
} Local;

// ---------------------------------------------------------------------------
// Types

typedef enum {
    TYPE_PATH,      // a.b.Name<args>
    TYPE_SELF,      // Self
    TYPE_REF,       // &T, &mut T
    TYPE_PTR,       // *T, *mut T
    TYPE_SLICE,     // [T]
    TYPE_ARRAY,     // [T; N]
    TYPE_TUPLE,     // (A, B), () is the unit type
    TYPE_FN,        // fn(A) R, move fn(A) R
    TYPE_DYN,       // dyn I
    TYPE_OPTION,    // T?
    TYPE_NEVER,     // !
} TypeKind;

typedef enum { MODE_BORROW, MODE_MUT, MODE_MOVE } Mode;

struct TypeExpr {
    TypeKind kind;
    Span span;
    union {
        struct { SymbolList segments; TypeList args; } path;
        struct { bool mutable; TypeExpr *inner; } ref;           // also pointers
        struct { TypeExpr *elem; Expr *len; } array;             // len is NULL for slices
        TypeList tuple;
        struct { TypeList params; List(Mode) modes; TypeExpr *ret; bool owned; } fn;
        TypeExpr *inner;                                         // dyn, option
    } as;
};

// ---------------------------------------------------------------------------
// Patterns

typedef enum {
    PAT_WILDCARD,   // _
    PAT_BINDING,    // name, mut name
    PAT_LITERAL,    // 1, "a", true, b'x'
    PAT_RANGE,      // 1..=9
    PAT_VARIANT,    // .Some(x), Option.Some(x), Color.Red
    PAT_TUPLE,      // (a, b)
    PAT_STRUCT,     // Point{x, y: 0, ..}
    PAT_TUPLE_STRUCT, // UserId(raw)
    PAT_OR,         // a | b
} PatternKind;

typedef struct {
    Symbol name;
    Pattern *pattern;   // NULL for the shorthand `x`
} FieldPattern;

struct Pattern {
    PatternKind kind;
    Span span;
    TypeId type;        // the type of the matched value, set by the checker
    union {
        struct { Symbol name; bool mutable; LocalId local; } binding;
        Expr *literal;
        struct { Expr *lo; Expr *hi; bool inclusive; } range;
        struct { SymbolList path; Symbol name; PatternList args; bool has_args;
                 uint32_t index; } variant;   // index: set by the checker
        PatternList tuple;
        struct { SymbolList path; List(FieldPattern) fields; bool rest; } struct_;
        struct { SymbolList path; PatternList args; } tuple_struct;
        PatternList alternatives;
    } as;
};

// ---------------------------------------------------------------------------
// Expressions

typedef enum {
    EXPR_INT, EXPR_FLOAT, EXPR_BYTE, EXPR_STR, EXPR_BOOL,
    EXPR_INTERP,        // "a {x} b": parts alternate between STR and expressions
    EXPR_IDENT,         // name
    EXPR_SELF,          // self
    EXPR_SELF_TYPE,     // Self (in Self.new())
    EXPR_GENERIC,       // Name<T, U> in expression position (spec 8.5)
    EXPR_UNARY,         // -x, !x, *x, &x, &mut x
    EXPR_BINARY,        // a + b, a ?? b, ...
    EXPR_CAST,          // x as T
    EXPR_RANGE,         // a..b, a..=b
    EXPR_CALL,          // f(a, name: b)
    EXPR_INDEX,         // a[i]
    EXPR_FIELD,         // a.name
    EXPR_TUPLE_FIELD,   // a.0
    EXPR_TRY,           // a?
    EXPR_STRUCT,        // Point{x: 1, ..base}
    EXPR_ARRAY,         // [a, b]
    EXPR_ARRAY_REPEAT,  // [value; count]
    EXPR_TUPLE,         // (a, b), () is unit
    EXPR_VARIANT,       // .Some(x), .None
    EXPR_IF,
    EXPR_MATCH,
    EXPR_BLOCK,
    EXPR_UNSAFE,        // unsafe { ... }
} ExprKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_REM,
    OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,   // bitwise
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_LOGICAL_AND, OP_LOGICAL_OR, OP_COALESCE,
    OP_NEG, OP_NOT, OP_DEREF, OP_REF, OP_REF_MUT,   // unary
    OP_ASSIGN,                                       // `=`, statements only
} Op;

typedef struct {
    Symbol name;        // 0 for a positional argument
    Expr *value;
} Arg;

typedef List(Arg) ArgList;

typedef struct {
    Symbol name;
    Expr *value;        // NULL for the shorthand `x`
} FieldInit;

typedef struct {
    Pattern *pattern;
    Expr *guard;        // NULL without `if`
    Expr *body;
} MatchArm;

struct Expr {
    ExprKind kind;
    Span span;
    TypeId type;            // set by the type checker
    union {
        uint64_t int_value;
        double float_value;
        uint8_t byte_value;
        bool bool_value;
        StrView str;
        ExprList parts;                                          // interpolation
        struct { Symbol name; Res res; } ident;                  // ident, self
        struct { Symbol name; TypeList args; Res res; } generic;
        struct { Op op; Expr *operand; } unary;
        struct { Op op; Expr *lhs; Expr *rhs; } binary;
        struct { Expr *value; TypeExpr *type; } cast;
        struct { Expr *lo; Expr *hi; bool inclusive; } range;    // lo, hi may be NULL
        struct { Expr *callee; ArgList args; } call;
        struct { Expr *base; Expr *index; } index;
        // `res` is set by the checker for a method: RES_DEF (a user method) or
        // RES_BUILTIN (a BuiltinMethod of a primitive type).
        struct { Expr *base; Symbol name; TypeList type_args; Res res; } field;
        struct { Expr *base; uint32_t index; } tuple_field;
        Expr *inner;                                             // try
        struct { Expr *type; List(FieldInit) fields; Expr *base; } struct_;
        ExprList elems;                                          // array, tuple
        struct { Expr *value; Expr *count; } repeat;
        struct { Symbol name; ArgList args; bool has_args; uint32_t index; } variant;
        struct { Pattern *let_pattern; Expr *cond; Block *then; Expr *else_; } if_;
        struct { Expr *scrutinee; List(MatchArm) arms; } match;
        Block *block;                                            // block, unsafe
    } as;
};

// ---------------------------------------------------------------------------
// Statements

typedef enum {
    STMT_LET,       // let pattern: T = value [else { ... }]
    STMT_VAR,       // var name: T = value
    STMT_ASSIGN,    // target op= value
    STMT_FOR,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_EXPR,
} StmtKind;

typedef enum {
    FOR_INFINITE,   // for { }
    FOR_COND,       // for cond { }
    FOR_IN,         // for [mut] x[, y] in expr { }
} ForKind;

struct Stmt {
    StmtKind kind;
    Span span;
    union {
        struct { Pattern *pattern; TypeExpr *type; Expr *value; Block *else_; } let;
        struct { Symbol name; TypeExpr *type; Expr *value; LocalId local; } var;
        // op is OP_ASSIGN or the operator of a compound assignment (+=, ...)
        struct { Op op; Expr *target; Expr *value; } assign;
        struct {
            ForKind kind;
            bool mutable;
            Symbol first, second;   // `for i, x in`: first = i, second = x
            LocalId first_local, second_local;
            Expr *expr;             // condition or iterated value
            Block *body;
        } for_;
        Expr *expr;                 // return value (may be NULL), expression statement
    } as;
};

struct Block {
    Span span;
    StmtList stmts;
};

// ---------------------------------------------------------------------------
// Items

typedef struct {
    Symbol name;
    List(Expr *) args;      // literals and identifiers
    Span span;
} Attribute;

typedef List(Attribute) AttributeList;

typedef struct {
    Symbol name;
    Mode mode;
    TypeExpr *type;
    Expr *default_value;    // NULL without `= value`
    Span span;
    LocalId local;
} Param;

struct FnDecl {
    Span span;
    Symbol name;
    AttributeList attributes;
    bool is_pub, is_unsafe;
    StrView abi;            // "C" for extern "C" fn, empty otherwise
    SymbolList type_params;
    bool has_self;
    Mode self_mode;
    List(Param) params;
    TypeExpr *ret;          // NULL for ()
    Block *body;            // NULL in extern blocks
    // Set by the resolver: `self` is local 0 in methods.
    DefId def;
    List(Local) locals;
    TypeId *local_types;    // set by the type checker, indexed by LocalId
};

typedef struct {
    Symbol name;
    AttributeList attributes;
    bool is_pub;
    TypeExpr *type;
    Expr *default_value;
    Span span;
} FieldDecl;

typedef struct {
    Symbol name;            // 0 for an unnamed payload field
    TypeExpr *type;
} VariantField;

typedef struct {
    Symbol name;
    List(VariantField) fields;
    bool has_fields;
    Expr *discriminant;
    Span span;
} Variant;

typedef enum {
    ITEM_FN,
    ITEM_STRUCT,
    ITEM_TUPLE_STRUCT,
    ITEM_ENUM,
    ITEM_CONST,
    ITEM_GLOBAL,
    ITEM_TYPE_ALIAS,
    ITEM_IMPORT,
    ITEM_EXTERN,
} ItemKind;

typedef struct Item {
    ItemKind kind;
    Span span;
    Symbol name;
    AttributeList attributes;
    bool is_pub;
    union {
        FnDecl *fn;
        struct {
            bool is_copy, is_ref;
            SymbolList type_params;
            List(FieldDecl) fields;     // tuple structs: unnamed fields
            FnList methods;
        } struct_;
        struct {
            bool is_copy;
            SymbolList type_params;
            List(Variant) variants;
            FnList methods;
        } enum_;
        struct { TypeExpr *type; Expr *value; } const_;           // const and global var
        struct { SymbolList type_params; TypeExpr *type; } alias;
        struct { SymbolList path; Symbol alias; } import;
        struct { StrView abi; FnList fns; } extern_;
    } as;
} Item;

typedef List(Item *) ItemList;

typedef struct {
    FileId file;
    List(StrView) docs;        // `//!` lines
    ItemList items;
} Module;

// Writes the AST in the indented format used by `--emit=ast` and tests/parse.
void print_module(const Module *module, const Interner *names, FILE *out);

#endif
