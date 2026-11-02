// `str` operations used by string interpolation.

#include "ofir_rt.h"

#include <string.h>

static void reserve(OfirStr *s, int64_t extra) {
    int64_t needed = s->len + extra;
    if (needed <= s->cap) {
        return;
    }
    int64_t cap = s->cap > 0 ? s->cap * 2 : 16;
    while (cap < needed) {
        cap *= 2;
    }
    if (s->cap == 0) {   // a literal or an empty string: copy to the heap
        uint8_t *heap = ofir_alloc(cap);
        if (s->len > 0) {
            memcpy(heap, s->ptr, (size_t)s->len);
        }
        s->ptr = heap;
    } else {
        s->ptr = ofir_realloc(s->ptr, cap);
    }
    s->cap = cap;
}

void ofir_str_push_bytes(OfirStr *s, const uint8_t *ptr, int64_t len) {
    if (len == 0) {
        return;
    }
    reserve(s, len);
    memcpy(s->ptr + s->len, ptr, (size_t)len);
    s->len += len;
}

void ofir_str_push_str(OfirStr *s, const OfirStr *part) {
    ofir_str_push_bytes(s, part->ptr, part->len);
}

void ofir_str_push_int(OfirStr *s, int64_t value) {
    char text[OFIR_FORMAT_SIZE];
    ofir_str_push_bytes(s, (const uint8_t *)text, ofir_format_int(text, value));
}

void ofir_str_push_uint(OfirStr *s, uint64_t value) {
    char text[OFIR_FORMAT_SIZE];
    ofir_str_push_bytes(s, (const uint8_t *)text, ofir_format_uint(text, value));
}

void ofir_str_push_float(OfirStr *s, double value) {
    char text[OFIR_FORMAT_SIZE];
    ofir_str_push_bytes(s, (const uint8_t *)text, ofir_format_float(text, value));
}

void ofir_str_push_f32(OfirStr *s, float value) {
    char text[OFIR_FORMAT_SIZE];
    ofir_str_push_bytes(s, (const uint8_t *)text, ofir_format_f32(text, value));
}

void ofir_str_push_bool(OfirStr *s, bool value) {
    ofir_str_push_bytes(s, (const uint8_t *)(value ? "true" : "false"), value ? 4 : 5);
}

void ofir_str_drop(OfirStr *s) {
    if (s->cap > 0) {
        ofir_free(s->ptr);
    }
    *s = (OfirStr){0};
}

void ofir_str_concat(OfirStr *out, const OfirStr *a, const OfirStr *b) {
    OfirStr result = {0};
    ofir_str_push_bytes(&result, a->ptr, a->len);
    ofir_str_push_bytes(&result, b->ptr, b->len);
    *out = result;
}

bool ofir_str_eq(const OfirStr *a, const OfirStr *b) {
    return a->len == b->len && (a->len == 0 || memcmp(a->ptr, b->ptr, (size_t)a->len) == 0);
}

int32_t ofir_str_cmp(const OfirStr *a, const OfirStr *b) {
    int64_t common = a->len < b->len ? a->len : b->len;
    int order = common > 0 ? memcmp(a->ptr, b->ptr, (size_t)common) : 0;
    if (order != 0) {
        return order < 0 ? -1 : 1;
    }
    return a->len < b->len ? -1 : a->len > b->len;
}
