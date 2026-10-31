#include "driver.h"

#include "session.h"
#include "syntax/lexer.h"

#include <stdio.h>

typedef enum {
    CONTINUE,   // go on with the next phase
    STOP,       // the requested output is done, or a phase failed
} Next;

static Next load_input(Session *session, const Options *opts, FileId *file) {
    if (!source_map_load(&session->sources, opts->input, file)) {
        fprintf(stderr, "error[E0900]: cannot read `%s`\n", opts->input);
        return STOP;
    }
    return CONTINUE;
}

static Next run_lexer(Session *session, const Options *opts, FileId file) {
    TokenList tokens = {0};
    lex(session, file, &tokens);
    if (opts->emit == EMIT_TOKENS) {
        print_tokens(&tokens, &session->sources, &session->names, stdout);
    }
    VEC_FREE(tokens);
    if (diags_has_errors(&session->diags) || opts->emit == EMIT_TOKENS) {
        return STOP;
    }
    return CONTINUE;
}

int driver_build(const Options *opts) {
    Session session;
    session_init(&session);
    FileId file;
    bool ok = load_input(&session, opts, &file) == CONTINUE;
    if (ok && run_lexer(&session, opts, file) == CONTINUE) {
        fprintf(stderr, "error: `%s` is not implemented yet (only --emit=tokens)\n", opts->command);
        ok = false;
    }
    diags_render(&session.diags, &session.sources, opts->format, stderr);
    ok = ok && !diags_has_errors(&session.diags);
    session_free(&session);
    return ok ? 0 : 1;
}
