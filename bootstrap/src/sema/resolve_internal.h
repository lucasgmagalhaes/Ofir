// State shared by the resolver files.

#ifndef OFIR_RESOLVE_INTERNAL_H
#define OFIR_RESOLVE_INTERNAL_H

#include "sema/sema.h"

typedef struct {
    Sema *sema;
    Symbol self_name;      // `self`, declared as local 0 of methods
    Symbol wildcard;       // `_`
    Vec(Local) locals;     // locals of the function being resolved
    const Item *owner;     // the struct or enum whose methods are resolved, or NULL
} Resolver;

void resolve_error(Resolver *r, const char *code, Span span, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

// For "%.*s" in messages: SV_ARG(resolver_name(r, name)).
StrView resolver_name(const Resolver *r, Symbol name);

void declare_item(Resolver *r, Def def);

// --emit=names (sema->trace): lines `line:col text` for the root module.
bool tracing(const Resolver *r);
void trace(Resolver *r, Span span, const char *format, ...) __attribute__((format(printf, 3, 4)));
// `line:col what name -> resolution`
void trace_use(Resolver *r, const char *what, Symbol name, Span span, Res res);

// resolve_attributes.c: spec 12.1
void check_attributes(Resolver *r, const Item *item);

// resolve_import.c
void resolve_import(Resolver *r, const Item *item);
void import_prelude(Resolver *r);   // after every module's declarations are known
// `m.name` where `m` is an imported module becomes a reference to the item.
void resolve_qualified(Resolver *r, Expr *e);
// The definition named `name` in module `index`, if visible from here.
DefId module_member(Resolver *r, uint32_t index, Symbol name, Span span);

// resolve_body.c
void resolve_fn(Resolver *r, FnDecl *fn);
void resolve_expr(Resolver *r, Expr *e);

#endif
