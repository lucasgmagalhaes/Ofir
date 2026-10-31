// State shared by the resolver files.

#ifndef ASTRA_RESOLVE_INTERNAL_H
#define ASTRA_RESOLVE_INTERNAL_H

#include "sema/sema.h"

typedef struct {
    Sema *sema;
    Symbol self_name;      // `self`, declared as local 0 of methods
    Symbol wildcard;       // `_`
    Vec(Local) locals;     // locals of the function being resolved
} Resolver;

void resolve_error(Resolver *r, const char *code, Span span, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

// For "%.*s" in messages: SV_ARG(resolver_name(r, name)).
StrView resolver_name(const Resolver *r, Symbol name);

// resolve_body.c
void resolve_fn(Resolver *r, FnDecl *fn);
void resolve_expr(Resolver *r, Expr *e);

#endif
