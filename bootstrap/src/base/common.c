#include "base/common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void *xmalloc(size_t size) {
    void *ptr = malloc(size ? size : 1);
    if (!ptr) {
        fatal("out of memory");
    }
    return ptr;
}

void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count ? count : 1, size ? size : 1);
    if (!ptr) {
        fatal("out of memory");
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size ? size : 1);
    if (!result) {
        fatal("out of memory");
    }
    return result;
}

void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fputs("ofir: internal error: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    abort();
}
