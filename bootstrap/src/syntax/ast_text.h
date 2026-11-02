// One-line text of types, patterns and literals (used by the AST printer).

#ifndef OFIR_AST_TEXT_H
#define OFIR_AST_TEXT_H

#include "base/str.h"
#include "syntax/ast.h"

void type_text(StrBuf *out, const TypeExpr *type, const Interner *names);
void pattern_text(StrBuf *out, const Pattern *pattern, const Interner *names);
void literal_text(StrBuf *out, const Expr *expr, const Interner *names);

#endif
