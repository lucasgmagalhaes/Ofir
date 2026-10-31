// The front end: loading, lexing, parsing and checking. The back end is in
// driver_backend.c.

#include "driver_internal.h"

#include "base/clock.h"
#include "syntax/lexer.h"
#include "syntax/parser.h"

#include <stdio.h>
#include <string.h>

void timer_start(Timer *timer) {
    timer->start = clock_now_ms();
}

void timer_report(Timer *timer, const char *phase, const char *detail) {
    if (timer->enabled) {
        fprintf(stderr, "time: %-8s %9.3f ms  %s\n", phase, clock_now_ms() - timer->start, detail);
    }
}

static Next run_lexer(Pipeline *p, TokenList *tokens) {
    Session *session = p->session;
    timer_start(p->timer);
    lex(session, p->file, tokens);
    char detail[96];
    const SourceFile *source = source_file(&session->sources, p->file);
    snprintf(detail, sizeof detail, "%zu bytes, %u lines, %zu tokens", source->text.len,
             source->line_count, tokens->len);
    timer_report(p->timer, "lex", detail);
    if (p->opts->emit == EMIT_TOKENS) {
        print_tokens(tokens, &session->sources, &session->names, stdout);
    }
    bool done = p->opts->emit == EMIT_TOKENS || p->opts->stop_after == STOP_AFTER_LEX;
    return diags_has_errors(&session->diags) || done ? STOP : CONTINUE;
}

static Next run_sema(Pipeline *p, Module *module) {
    timer_start(p->timer);
    Sema sema;
    sema_init(&sema, p->session, module);
    resolve_module(&sema);
    if (!diags_has_errors(&p->session->diags)) {
        check_module(&sema);
    }
    char detail[64];
    snprintf(detail, sizeof detail, "%u types", sema.types.count);
    timer_report(p->timer, "check", detail);
    Next next = STOP;
    if (!diags_has_errors(&p->session->diags) && p->opts->stop_after != STOP_AFTER_CHECK) {
        next = run_backend(p, &sema);
    }
    sema_free(&sema);
    return next;
}

static Next run_parser(Pipeline *p, TokenList *tokens) {
    timer_start(p->timer);
    Module *module = parse(p->session, p->file, tokens);
    char detail[64];
    snprintf(detail, sizeof detail, "%u items", module->items.len);
    timer_report(p->timer, "parse", detail);
    if (p->opts->emit == EMIT_AST) {
        print_module(module, &p->session->names, stdout);
    }
    bool done = p->opts->emit == EMIT_AST || p->opts->stop_after == STOP_AFTER_PARSE;
    if (diags_has_errors(&p->session->diags) || done) {
        return STOP;
    }
    return run_sema(p, module);
}

// Runs the phases in order. Returns whether every requested phase finished.
static bool run_phases(Pipeline *p) {
    timer_start(p->timer);
    if (!source_map_load(&p->session->sources, p->opts->input, &p->file)) {
        fprintf(stderr, "error[E0900]: cannot read `%s`\n", p->opts->input);
        return false;
    }
    timer_report(p->timer, "load", "");
    TokenList tokens = {0};
    Next next = run_lexer(p, &tokens);
    if (next == CONTINUE) {
        next = run_parser(p, &tokens);
    }
    VEC_FREE(tokens);
    return next != STOP_FAIL && !diags_has_errors(&p->session->diags);
}

int driver_build(const Options *opts) {
    Timer total = {.enabled = opts->time_passes};
    Timer phase = total;
    timer_start(&total);
    Session session;
    session_init(&session);
    Pipeline p = {.session = &session, .opts = opts, .timer = &phase};
    bool ok = run_phases(&p);
    diags_render(&session.diags, &session.sources, opts->format, stderr);
    session_free(&session);
    timer_report(&total, "total", "");
    if (ok && p.exe[0] && strcmp(opts->command, "run") == 0) {
        return run_program(p.exe);
    }
    return ok ? 0 : 1;
}
