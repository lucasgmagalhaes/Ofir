// String views and string builders.

#ifndef ASTRA_STR_H
#define ASTRA_STR_H

#include "base/common.h"

// A borrowed, not necessarily zero-terminated, sequence of bytes.
typedef struct {
    const char *ptr;
    size_t len;
} StrView;

#define SV(literal) ((StrView){(literal), sizeof(literal) - 1})
#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).len, (sv).ptr

StrView sv_from_cstr(const char *s);
bool sv_eq(StrView a, StrView b);
bool sv_eq_cstr(StrView a, const char *b);
uint64_t sv_hash(StrView s);

// A growable, zero-terminated byte buffer.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

void strbuf_free(StrBuf *buf);
void strbuf_clear(StrBuf *buf);
void strbuf_push(StrBuf *buf, char c);
void strbuf_append(StrBuf *buf, StrView s);
void strbuf_append_cstr(StrBuf *buf, const char *s);
void strbuf_appendf(StrBuf *buf, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
StrView strbuf_view(const StrBuf *buf);

#endif
