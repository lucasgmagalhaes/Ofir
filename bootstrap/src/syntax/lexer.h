// Lexer: source text → tokens (spec chapter 1).

#ifndef ASTRA_LEXER_H
#define ASTRA_LEXER_H

#include "diag/diag.h"
#include "syntax/token.h"

// Appends the tokens of `file` to `out`, ending with TOK_EOF. Strings decoded
// from escapes are allocated in `arena`. Errors go to `diags`; the lexer
// always produces a complete token list so that parsing can continue.
void lex(const SourceMap *map, FileId file, Interner *names, Arena *arena, Diagnostics *diags,
         TokenList *out);

// Writes one token per line, as used by `--emit=tokens` and tests/lex.
void print_tokens(const TokenList *tokens, const SourceMap *map, const Interner *names, FILE *out);

#endif
