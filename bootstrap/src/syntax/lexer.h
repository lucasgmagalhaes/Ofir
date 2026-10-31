// Lexer: source text → tokens (spec chapter 1).

#ifndef ASTRA_LEXER_H
#define ASTRA_LEXER_H

#include "session.h"
#include "syntax/token.h"

// Appends the tokens of `file` to `out`, ending with TOK_EOF. Decoded string
// contents are allocated in the session arena. Errors go to the session
// diagnostics; the token list is always complete so parsing can continue.
void lex(Session *session, FileId file, TokenList *out);

#endif
