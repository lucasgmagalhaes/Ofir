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

static Next run_lexer(Pipeline *p, FileId file, TokenList *tokens, bool root) {
    Session *session = p->session;
    timer_start(p->timer);
    lex(session, file, tokens);
    char detail[96];
    const SourceFile *source = source_file(&session->sources, file);
    snprintf(detail, sizeof detail, "%zu bytes, %u lines, %zu tokens", source->text.len,
             source->line_count, tokens->len);
    timer_report(p->timer, "lex", detail);
    if (root && p->opts->emit == EMIT_TOKENS) {
        print_tokens(tokens, &session->sources, &session->names, stdout);
    }
    bool done = p->opts->emit == EMIT_TOKENS || p->opts->stop_after == STOP_AFTER_LEX;
    return diags_has_errors(&session->diags) || done ? STOP : CONTINUE;
}

Module *parse_file(Pipeline *p, FileId file, bool root, Next *next) {
    TokenList tokens = {0};
    *next = run_lexer(p, file, &tokens, root);
    Module *module = NULL;
    if (*next == CONTINUE) {
        timer_start(p->timer);
        module = parse(p->session, file, &tokens);
        char detail[64];
        snprintf(detail, sizeof detail, "%u items", module->items.len);
        timer_report(p->timer, "parse", detail);
        if (root && p->opts->emit == EMIT_AST) {
            print_module(module, &p->session->names, stdout);
        }
        bool done = p->opts->emit == EMIT_AST || p->opts->stop_after == STOP_AFTER_PARSE;
        *next = diags_has_errors(&p->session->diags) || done ? STOP : CONTINUE;
    }
    VEC_FREE(tokens);
    return module;
}

static Next run_sema(Pipeline *p, Program program) {
    timer_start(p->timer);
    Sema sema;
    sema_init(&sema, p->session, program);
    sema.testing = strcmp(p->opts->command, "test") == 0;
    resolve_program(&sema);
    if (!diags_has_errors(&p->session->diags)) {
        check_program(&sema);
    }
    char detail[64];
    snprintf(detail, sizeof detail, "%u modules, %u types", program.len, sema.types.count);
    timer_report(p->timer, "check", detail);
    Next next = STOP;
    if (!diags_has_errors(&p->session->diags) && p->opts->stop_after != STOP_AFTER_CHECK) {
        next = run_backend(p, &sema);
        keep_test_names(p, &sema);
    }
    sema_free(&sema);
    return next;
}

// Runs the phases in order. Returns whether every requested phase finished.
static bool run_phases(Pipeline *p) {
    timer_start(p->timer);
    if (!source_map_load(&p->session->sources, p->opts->input, &p->file)) {
        fprintf(stderr, "error[E0900]: cannot read `%s`\n", p->opts->input);
        return false;
    }
    timer_report(p->timer, "load", "");
    Next next;
    Module *root = parse_file(p, p->file, true, &next);
    if (next == CONTINUE) {
        Program program = load_program(p, root);
        next = diags_has_errors(&p->session->diags) ? STOP : run_sema(p, program);
    }
    return next != STOP_FAIL && !diags_has_errors(&p->session->diags);
}

int driver_build(const Options *opts) {
    Timer total = {.enabled = opts->time_passes};
    Timer phase = total;
    timer_start(&total);
    Session session;
    session_init(&session);
    session.diags.deny_warnings = opts->deny_warnings;
    Pipeline p = {.session = &session, .opts = opts, .timer = &phase};
    bool ok = run_phases(&p);
    diags_render(&session.diags, &session.sources, opts->format, stderr);
    session_free(&session);
    timer_report(&total, "total", "");
    int status = ok ? 0 : 1;
    if (ok && p.exe[0] && strcmp(opts->command, "run") == 0) {
        status = run_program(p.exe, NULL);
    } else if (ok && p.exe[0] && strcmp(opts->command, "test") == 0) {
        status = run_tests(&p);
    }
    free_test_names(&p);
    return status;
}
