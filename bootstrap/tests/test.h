// Minimal unit test helpers for the stage 0 C code.

#ifndef ASTRA_TEST_H
#define ASTRA_TEST_H

#include <stdio.h>

static int test_failures = 0;
static int test_checks = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        test_checks++;                                                      \
        if (!(cond)) {                                                      \
            test_failures++;                                                \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                   \
    } while (0)

static inline int test_report(const char *name) {
    printf("%s: %d checks, %d failed\n", name, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

#endif
