// A fast buffered text writer for the generated code. `out_fmt` accepts a
// small subset of printf: %s %c %d %u %zu %ld %lu %lld %llu, zero-padded
// upper-case hex (%02X, %016lX, %016llX) and %%. It avoids the cost of the C
// library's printf, which dominated code generation.

#ifndef ASTRA_OUT_H
#define ASTRA_OUT_H

#include "base/str.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *file;       // flushed to this file; NULL for a memory buffer
    char *buf;
    size_t len, cap;
    bool fixed;       // a caller's array: never grows, truncates
} Out;

void out_to_file(Out *out, FILE *file);
void out_to_memory(Out *out);                      // grows as needed
void out_to_array(Out *out, char *array, size_t size);   // NUL-terminated, truncated
void out_flush(Out *out);
void out_free(Out *out);

void out_write_slow(Out *out, const char *text, size_t len);

// The fast path is inline: most pieces are a few bytes. The byte after the
// text is set to NUL (there is always room: the test is strict).
static inline void out_write(Out *out, const char *text, size_t len) {
    if (out->len + len < out->cap) {
        memcpy(out->buf + out->len, text, len);
        out->len += len;
        out->buf[out->len] = '\0';
        return;
    }
    out_write_slow(out, text, len);
}

static inline void out_str(Out *out, const char *text) {
    out_write(out, text, strlen(text));
}

static inline void out_char(Out *out, char c) {
    out_write(out, &c, 1);
}

void out_u64(Out *out, uint64_t value);
void out_i64(Out *out, int64_t value);
void out_fmt(Out *out, const char *format, ...) __attribute__((format(printf, 2, 3)));

// Formats into a fixed array, like snprintf.
void fmt_into(char *array, size_t size, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

#endif
