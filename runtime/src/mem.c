// Memory allocation: wraps malloc and panics when out of memory.

#include "astra_rt.h"

#include <stdlib.h>
#include <string.h>

static _Noreturn void out_of_memory(void) {
    static const char msg[] = "out of memory";
    astra_panic((const uint8_t *)msg, (int64_t)strlen(msg), NULL);
}

void *astra_alloc(int64_t size) {
    void *ptr = malloc(size > 0 ? (size_t)size : 1);
    if (!ptr) {
        out_of_memory();
    }
    return ptr;
}

void *astra_realloc(void *ptr, int64_t size) {
    void *grown = realloc(ptr, size > 0 ? (size_t)size : 1);
    if (!grown) {
        out_of_memory();
    }
    return grown;
}

void astra_free(void *ptr) {
    free(ptr);
}
