// Panics (docs/design/runtime.md, 2). All functions are cold and noreturn.

#include "ofir_rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLD __attribute__((cold, noinline))

COLD _Noreturn void ofir_panic(const uint8_t *msg, int64_t len, const OfirLoc *loc) {
    ofir_stdout_flush();   // keep the program's output before the message
    fprintf(stderr, "panic: %.*s\n", (int)len, (const char *)msg);
    if (loc) {
        fprintf(stderr, "  at %s:%u:%u\n", loc->file, loc->line, loc->col);
    }
    exit(OFIR_PANIC_STATUS);
}

static COLD _Noreturn void panic_text(const char *msg, const OfirLoc *loc) {
    ofir_panic((const uint8_t *)msg, (int64_t)strlen(msg), loc);
}

COLD _Noreturn void ofir_panic_overflow(const OfirLoc *loc) {
    panic_text("integer overflow", loc);
}

COLD _Noreturn void ofir_panic_div_zero(const OfirLoc *loc) {
    panic_text("division by zero", loc);
}

COLD _Noreturn void ofir_panic_shift(const OfirLoc *loc) {
    panic_text("shift amount out of range", loc);
}

COLD _Noreturn void ofir_panic_cast(const OfirLoc *loc) {
    panic_text("value out of range for the target type of `as`", loc);
}

COLD _Noreturn void ofir_panic_bounds(int64_t index, int64_t len, const OfirLoc *loc) {
    char msg[96];
    int n = snprintf(msg, sizeof msg,
                     "index out of bounds: the length is %lld but the index is %lld",
                     (long long)len, (long long)index);
    ofir_panic((const uint8_t *)msg, n, loc);
}

COLD _Noreturn void ofir_panic_assert(const OfirLoc *loc) {
    panic_text("assertion failed", loc);
}

COLD _Noreturn void ofir_panic_str(const OfirStr *msg, const OfirLoc *loc) {
    ofir_panic(msg->ptr, msg->len, loc);
}
