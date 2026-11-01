// `astra test` (spec 12.2): the program built with the `@test` functions
// as roots runs once per test, with the test's index as its argument, so
// that a panic fails only that test.

#include "driver_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void keep_test_names(Pipeline *p, const Sema *sema) {
    p->test_count = sema->tests.len;
    p->tests = p->test_count ? calloc(p->test_count, sizeof(char *)) : NULL;
    for (size_t i = 0; i < p->test_count; i++) {
        const FnDecl *fn = sema->tests.data[i];
        StrView module = sema->program.items[sema_def((Sema *)sema, fn->def)->module].name;
        StrView name = symbol_name(&sema->session->names, fn->name);
        size_t size = module.len + name.len + 2;
        p->tests[i] = malloc(size);
        snprintf(p->tests[i], size, SV_FMT "." SV_FMT, SV_ARG(module), SV_ARG(name));
    }
}

void free_test_names(Pipeline *p) {
    for (size_t i = 0; i < p->test_count; i++) {
        free(p->tests[i]);
    }
    free(p->tests);
    p->tests = NULL;
    p->test_count = 0;
}

int run_tests(const Pipeline *p) {
    size_t failed = 0;
    for (size_t i = 0; i < p->test_count; i++) {
        char index[24];
        snprintf(index, sizeof index, "%zu", i);
        printf("test %s ... ", p->tests[i]);
        fflush(stdout);
        int status = run_program(p->exe, index);
        if (status == 0) {
            printf("ok\n");
        } else {
            printf("FAILED (exit status %d)\n", status);
            failed++;
        }
    }
    printf("\n%zu tests: %zu passed, %zu failed\n", p->test_count, p->test_count - failed, failed);
    return failed ? 1 : 0;
}
