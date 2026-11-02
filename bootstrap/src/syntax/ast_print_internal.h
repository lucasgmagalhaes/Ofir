// Helpers shared by the files of the AST printer.

#ifndef OFIR_AST_PRINT_INTERNAL_H
#define OFIR_AST_PRINT_INTERNAL_H

#include "base/str.h"
#include "syntax/ast.h"

typedef struct {
    FILE *out;
    const Interner *names;
    int depth;
} Printer;

void line_start(Printer *p);
void line(Printer *p, const char *text);
void text_line(Printer *p, const char *label, const StrBuf *text);
void type_line(Printer *p, const char *label, const TypeExpr *t);
void pattern_line(Printer *p, const char *label, const Pattern *pat);
const char *name(const Printer *p, Symbol s);

void print_expr(Printer *p, const Expr *e);
void print_block(Printer *p, const char *label, const Block *b);

#endif
