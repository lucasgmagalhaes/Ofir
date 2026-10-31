#include "driver.h"

#include "base/clock.h"
#include "session.h"
#include "syntax/lexer.h"

#include <stdio.h>

// Times each phase when `--time-passes` is given.
typedef struct {
    bool enabled;
    double start;   // when the current phase started
} Timer;

static void timer_start(Timer *timer) {
    timer->start = clock_now_ms();
}

static void timer_report(Timer *timer, const char *phase, const char *detail) {
    if (timer->enabled) {
        fprintf(stderr, "time: %-8s %9.3f ms  %s\n", phase, clock_now_ms() - timer->start, detail);
    }
}

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

static Next run_lexer(Session *session, const Options *opts, FileId file, Timer *timer) {
    TokenList tokens = {0};
    timer_start(timer);
    lex(session, file, &tokens);
    char detail[96];
    const SourceFile *source = source_file(&session->sources, file);
    snprintf(detail, sizeof detail, "%zu bytes, %u lines, %zu tokens", source->text.len,
             source->line_count, tokens.len);
    timer_report(timer, "lex", detail);
    if (opts->emit == EMIT_TOKENS) {
        print_tokens(&tokens, &session->sources, &session->names, stdout);
    }
    VEC_FREE(tokens);
    bool done = opts->emit == EMIT_TOKENS || opts->stop_after == STOP_AFTER_LEX;
    if (diags_has_errors(&session->diags) || done) {
        return STOP;
    }
    return CONTINUE;
}

int driver_build(const Options *opts) {
    Timer total = {.enabled = opts->time_passes};
    Timer phase = total;
    timer_start(&total);
    Session session;
    session_init(&session);
    FileId file;
    timer_start(&phase);
    bool ok = load_input(&session, opts, &file) == CONTINUE;
    timer_report(&phase, "load", "");
    if (ok && run_lexer(&session, opts, file, &phase) == CONTINUE) {
        // The phases after the lexer are not written yet.
        fprintf(stderr, "error: `%s` is not implemented yet (only --emit=tokens)\n", opts->command);
        ok = false;
    }
    diags_render(&session.diags, &session.sources, opts->format, stderr);
    ok = ok && !diags_has_errors(&session.diags);
    session_free(&session);
    timer_report(&total, "total", "");
    return ok ? 0 : 1;
}
