#include "base/intern.h"
#include "base/str.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_views(void) {
    CHECK(sv_eq(SV("abc"), sv_from_cstr("abc")));
    CHECK(!sv_eq(SV("abc"), SV("abd")));
    CHECK(sv_eq(SV(""), (StrView){NULL, 0}));
    CHECK(sv_hash(SV("abc")) == sv_hash(sv_from_cstr("abc")));
}

static void test_builder(void) {
    StrBuf buf = {0};
    strbuf_append_cstr(&buf, "x = ");
    strbuf_appendf(&buf, "%d", 42);
    strbuf_push(&buf, '!');
    CHECK(strcmp(buf.data, "x = 42!") == 0);
    for (int i = 0; i < 1000; i++) {
        strbuf_push(&buf, 'a');
    }
    CHECK(buf.len == 1007);
    strbuf_clear(&buf);
    CHECK(buf.len == 0 && buf.data[0] == '\0');
    strbuf_free(&buf);
}

static void test_interner(void) {
    Arena arena;
    arena_init(&arena);
    Interner interner;
    interner_init(&interner, &arena);

    Symbol a = intern(&interner, SV("alpha"));
    Symbol b = intern(&interner, SV("beta"));
    CHECK(a.id != 0 && b.id != 0 && a.id != b.id);
    CHECK(symbol_eq(a, intern(&interner, SV("alpha"))));
    CHECK(sv_eq(symbol_name(&interner, b), SV("beta")));

    // Force several table growths and check every name survives.
    char name[32];
    Symbol syms[5000];
    for (int i = 0; i < 5000; i++) {
        snprintf(name, sizeof name, "name%d", i);
        syms[i] = intern(&interner, sv_from_cstr(name));
    }
    int mismatches = 0;
    for (int i = 0; i < 5000; i++) {
        snprintf(name, sizeof name, "name%d", i);
        if (!symbol_eq(syms[i], intern(&interner, sv_from_cstr(name)))) {
            mismatches++;
        }
    }
    CHECK(mismatches == 0);
    CHECK(symbol_eq(a, intern(&interner, SV("alpha"))));

    interner_free(&interner);
    arena_free(&arena);
}

int main(void) {
    test_views();
    test_builder();
    test_interner();
    return test_report("str");
}
