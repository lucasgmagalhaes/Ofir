#include "driver.h"

#include "base/clock.h"
#include "session.h"
#include "syntax/lexer.h"
#include "sema/sema.h"
#include "syntax/parser.h"

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

static Next run_lexer(Session *session, const Options *opts, FileId file, TokenList *tokens,
                      Timer *timer) {
    timer_start(timer);
    lex(session, file, tokens);
    char detail[96];
    const SourceFile *source = source_file(&session->sources, file);
    snprintf(detail, sizeof detail, "%zu bytes, %u lines, %zu tokens", source->text.len,
             source->line_count, tokens->len);
    timer_report(timer, "lex", detail);
    if (opts->emit == EMIT_TOKENS) {
        print_tokens(tokens, &session->sources, &session->names, stdout);
    }
    bool done = opts->emit == EMIT_TOKENS || opts->stop_after == STOP_AFTER_LEX;
    if (diags_has_errors(&session->diags) || done) {
        return STOP;
    }
    return CONTINUE;
}

static Next run_sema(Session *session, const Options *opts, Module *module, Timer *timer) {
    timer_start(timer);
    Sema sema;
    sema_init(&sema, session, module);
    resolve_module(&sema);
    if (!diags_has_errors(&session->diags)) {
        check_module(&sema);
    }
    char detail[64];
    snprintf(detail, sizeof detail, "%u types", sema.types.count);
    sema_free(&sema);
    timer_report(timer, "check", detail);
    bool done = opts->stop_after == STOP_AFTER_CHECK;
    return diags_has_errors(&session->diags) || done ? STOP : CONTINUE;
}

static Next run_parser(Session *session, const Options *opts, FileId file,
                       TokenList *tokens, Timer *timer) {
    timer_start(timer);
    Module *module = parse(session, file, tokens);
    char detail[64];
    snprintf(detail, sizeof detail, "%u items", module->items.len);
    timer_report(timer, "parse", detail);
    if (opts->emit == EMIT_AST) {
        print_module(module, &session->names, stdout);
    }
    bool done = opts->emit == EMIT_AST || opts->stop_after == STOP_AFTER_PARSE;
    if (diags_has_errors(&session->diags) || done) {
        return STOP;
    }
    return run_sema(session, opts, module, timer);
}

// Runs the phases in order; returns false if one of them could not finish.
static bool run_phases(Session *session, const Options *opts, Timer *timer) {
    FileId file;
    timer_start(timer);
    bool ok = load_input(session, opts, &file) == CONTINUE;
    timer_report(timer, "load", "");
    if (!ok) {
        return false;
    }
    TokenList tokens = {0};
    Next next = run_lexer(session, opts, file, &tokens, timer);
    if (next == CONTINUE) {
        next = run_parser(session, opts, file, &tokens, timer);
    }
    VEC_FREE(tokens);
    if (next == CONTINUE) {
        // The phases after the parser are not written yet.
        fprintf(stderr, "error: `%s` is not implemented yet (only --emit=tokens|ast)\n",
                opts->command);
        return false;
    }
    return true;
}

int driver_build(const Options *opts) {
    Timer total = {.enabled = opts->time_passes};
    Timer phase = total;
    timer_start(&total);
    Session session;
    session_init(&session);
    bool ok = run_phases(&session, opts, &phase);
    diags_render(&session.diags, &session.sources, opts->format, stderr);
    ok = ok && !diags_has_errors(&session.diags);
    session_free(&session);
    timer_report(&total, "total", "");
    return ok ? 0 : 1;
}
