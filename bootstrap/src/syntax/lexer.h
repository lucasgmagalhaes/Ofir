// Lexer: source text → tokens (spec chapter 1).

#ifndef OFIR_LEXER_H
#define OFIR_LEXER_H

#include "session.h"
#include "syntax/token.h"

// Appends the tokens of `file` to `out`, ending with TOK_EOF. Decoded string
// contents are allocated in the session arena. Errors go to the session
// diagnostics; the token list is always complete so parsing can continue.
void lex(Session *session, FileId file, TokenList *out);

// Interns every keyword. Must run on a fresh interner, before any other
// name; session_init does it.
void intern_keywords(Interner *names);

#endif
