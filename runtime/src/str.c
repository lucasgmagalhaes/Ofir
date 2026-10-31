// `str` operations used by string interpolation.

#include "astra_rt.h"

#include <string.h>

static void reserve(AstraStr *s, int64_t extra) {
    int64_t needed = s->len + extra;
    if (needed <= s->cap) {
        return;
    }
    int64_t cap = s->cap > 0 ? s->cap * 2 : 16;
    while (cap < needed) {
        cap *= 2;
    }
    if (s->cap == 0) {   // a literal or an empty string: copy to the heap
        uint8_t *heap = astra_alloc(cap);
        if (s->len > 0) {
            memcpy(heap, s->ptr, (size_t)s->len);
        }
        s->ptr = heap;
    } else {
        s->ptr = astra_realloc(s->ptr, cap);
    }
    s->cap = cap;
}

void astra_str_push_bytes(AstraStr *s, const uint8_t *ptr, int64_t len) {
    if (len == 0) {
        return;
    }
    reserve(s, len);
    memcpy(s->ptr + s->len, ptr, (size_t)len);
    s->len += len;
}

void astra_str_push_str(AstraStr *s, const AstraStr *part) {
    astra_str_push_bytes(s, part->ptr, part->len);
}

void astra_str_push_int(AstraStr *s, int64_t value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_str_push_bytes(s, (const uint8_t *)text, astra_format_int(text, value));
}

void astra_str_push_uint(AstraStr *s, uint64_t value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_str_push_bytes(s, (const uint8_t *)text, astra_format_uint(text, value));
}

void astra_str_push_float(AstraStr *s, double value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_str_push_bytes(s, (const uint8_t *)text, astra_format_float(text, value));
}

void astra_str_push_f32(AstraStr *s, float value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_str_push_bytes(s, (const uint8_t *)text, astra_format_f32(text, value));
}

void astra_str_push_bool(AstraStr *s, bool value) {
    astra_str_push_bytes(s, (const uint8_t *)(value ? "true" : "false"), value ? 4 : 5);
}

void astra_str_drop(AstraStr *s) {
    if (s->cap > 0) {
        astra_free(s->ptr);
    }
    *s = (AstraStr){0};
}

void astra_str_concat(AstraStr *out, const AstraStr *a, const AstraStr *b) {
    AstraStr result = {0};
    astra_str_push_bytes(&result, a->ptr, a->len);
    astra_str_push_bytes(&result, b->ptr, b->len);
    *out = result;
}

bool astra_str_eq(const AstraStr *a, const AstraStr *b) {
    return a->len == b->len && (a->len == 0 || memcmp(a->ptr, b->ptr, (size_t)a->len) == 0);
}

int32_t astra_str_cmp(const AstraStr *a, const AstraStr *b) {
    int64_t common = a->len < b->len ? a->len : b->len;
    int order = common > 0 ? memcmp(a->ptr, b->ptr, (size_t)common) : 0;
    if (order != 0) {
        return order < 0 ? -1 : 1;
    }
    return a->len < b->len ? -1 : a->len > b->len;
}
