// Parser: tokens → AST (spec appendix A).

#ifndef ASTRA_PARSER_H
#define ASTRA_PARSER_H

#include "session.h"
#include "syntax/ast.h"
#include "syntax/token.h"

// Parses one file. Syntax errors go to the session diagnostics; after an
// error the parser skips to the next statement and continues, so several
// independent errors are reported per file. Doc comment tokens are removed
// from `tokens`, in place.
Module *parse(Session *session, FileId file, TokenList *tokens);

#endif
