#include "diag/diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void diags_init(Diagnostics *diags, Arena *arena) {
    diags->arena = arena;
    diags->items.data = NULL;
    diags->items.len = diags->items.cap = 0;
    diags->error_count = 0;
}

void diags_free(Diagnostics *diags) {
    VEC_FREE(diags->items);
}

// Formats the message of `d` and records it.
static void add(Diagnostics *diags, Diagnostic d, const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    d.message = arena_alloc(diags->arena, (size_t)n + 1, 1);
    vsnprintf(d.message, (size_t)n + 1, format, args);
    VEC_PUSH(diags->items, d);
    if (d.severity == SEVERITY_ERROR) {
        diags->error_count++;
    }
}

static Diagnostic make(Severity severity, const char *code, Span span) {
    Diagnostic d = {.severity = severity, .span = span};
    snprintf(d.code, sizeof d.code, "%s", code);
    return d;
}

void diag_error(Diagnostics *diags, const char *code, Span span, const char *format, ...) {
    va_list args;
    va_start(args, format);
    add(diags, make(SEVERITY_ERROR, code, span), format, args);
    va_end(args);
}

void diag_warning(Diagnostics *diags, const char *code, Span span, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Severity severity = diags->deny_warnings ? SEVERITY_ERROR : SEVERITY_WARNING;
    add(diags, make(severity, code, span), format, args);
    va_end(args);
}

static int compare(const void *pa, const void *pb) {
    const Diagnostic *a = pa, *b = pb;
    if (a->span.file.id != b->span.file.id) {
        return a->span.file.id < b->span.file.id ? -1 : 1;
    }
    if (a->span.start != b->span.start) {
        return a->span.start < b->span.start ? -1 : 1;
    }
    return strcmp(a->code, b->code);
}

static bool same(const Diagnostic *a, const Diagnostic *b) {
    return a->span.file.id == b->span.file.id && a->span.start == b->span.start &&
           strcmp(a->code, b->code) == 0 && strcmp(a->message, b->message) == 0;
}

static const char *severity_name(Severity s) {
    return s == SEVERITY_ERROR ? "error" : "warning";
}

static void render_human(const Diagnostic *d, const SourceMap *map, FILE *out) {
    const SourceFile *file = source_file(map, d->span.file);
    LineCol lc = source_line_col(map, d->span.file, d->span.start);
    StrView line = source_line_text(map, d->span.file, lc.line);

    char number[16];
    int width = snprintf(number, sizeof number, "%u", lc.line);
    fprintf(out, "%s[%s]: %s\n", severity_name(d->severity), d->code, d->message);
    fprintf(out, "%*s--> " SV_FMT ":%u:%u\n", width, "", SV_ARG(file->path), lc.line, lc.column);
    fprintf(out, "%*s |\n", width, "");
    fprintf(out, "%s | " SV_FMT "\n", number, SV_ARG(line));

    // The caret line: tabs are kept so the marker stays under the text.
    uint32_t len = d->span.end > d->span.start ? d->span.end - d->span.start : 1;
    if (lc.column - 1 + len > line.len + 1) {
        len = (uint32_t)line.len + 1 - (lc.column - 1);   // clip spans running past the line
    }
    fprintf(out, "%*s | ", width, "");
    for (uint32_t i = 0; i + 1 < lc.column; i++) {
        fputc(line.ptr[i] == '\t' ? '\t' : ' ', out);
    }
    for (uint32_t i = 0; i < len; i++) {
        fputc('^', out);
    }
    fputs("\n\n", out);
}

void diags_render(Diagnostics *diags, const SourceMap *map, DiagFormat format, FILE *out) {
    if (diags->items.len > 1) {
        qsort(diags->items.data, diags->items.len, sizeof(Diagnostic), compare);
    }
    for (size_t i = 0; i < diags->items.len; i++) {
        const Diagnostic *d = &diags->items.data[i];
        if (i > 0 && same(d, &diags->items.data[i - 1])) {
            continue;
        }
        if (format == DIAG_FORMAT_SHORT) {
            const SourceFile *file = source_file(map, d->span.file);
            LineCol lc = source_line_col(map, d->span.file, d->span.start);
            fprintf(out, SV_FMT ":%u:%u: %s[%s]: %s\n", SV_ARG(file->path), lc.line, lc.column,
                    severity_name(d->severity), d->code, d->message);
        } else {
            render_human(d, map, out);
        }
    }
}
