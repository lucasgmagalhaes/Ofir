// Growable arrays.
//
//     Vec(int) numbers = {0};
//     VEC_PUSH(numbers, 42);
//     for (size_t i = 0; i < numbers.len; i++) { ... numbers.data[i] ... }
//     VEC_FREE(numbers);

#ifndef OFIR_VEC_H
#define OFIR_VEC_H

#include "base/common.h"

#include <stdlib.h>

#define Vec(T) struct { T *data; size_t len; size_t cap; }

#define VEC_RESERVE(v, n)                                                   \
    do {                                                                    \
        if ((v).len + (n) > (v).cap) {                                      \
            size_t vec_new_cap_ = (v).cap ? (v).cap * 2 : 8;                \
            while (vec_new_cap_ < (v).len + (n)) vec_new_cap_ *= 2;         \
            (v).data = xrealloc((v).data, vec_new_cap_ * sizeof(*(v).data)); \
            (v).cap = vec_new_cap_;                                         \
        }                                                                   \
    } while (0)

#define VEC_PUSH(v, x)                \
    do {                              \
        VEC_RESERVE((v), 1);          \
        (v).data[(v).len++] = (x);    \
    } while (0)

#define VEC_POP(v) ((v).data[--(v).len])
#define VEC_LAST(v) ((v).data[(v).len - 1])

#define VEC_FREE(v)          \
    do {                     \
        free((v).data);      \
        (v).data = NULL;     \
        (v).len = (v).cap = 0; \
    } while (0)

#endif
