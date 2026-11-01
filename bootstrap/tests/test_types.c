#include "sema/types.h"
#include "test.h"

#include <string.h>

static bool text_is(const TypeTable *table, TypeId id, const char *expected) {
    StrBuf buf = {0};
    ty_text(&buf, table, id, NULL, NULL);
    bool ok = strcmp(buf.data, expected) == 0;
    if (!ok) {
        fprintf(stderr, "  got `%s`, expected `%s`\n", buf.data, expected);
    }
    strbuf_free(&buf);
    return ok;
}

static void test_builtins(TypeTable *table) {
    CHECK(type_get(table, type_id(TID_ERROR))->kind == TY_ERROR);
    CHECK(type_get(table, type_id(TID_U8))->kind == TY_INT);
    CHECK(type_get(table, type_id(TID_U8))->data == INT_U8);
    CHECK(type_get(table, type_id(TID_F32))->data == 32);
    CHECK(type_eq(type_intern(table, (Ty){.kind = TY_STR}), type_id(TID_STR)));
    CHECK(type_is_integer(table, type_id(TID_INT)));
    CHECK(!type_is_integer(table, type_id(TID_FLOAT)));
    CHECK(type_is_numeric(table, type_id(TID_FLOAT)));
}

static void test_interning(TypeTable *table) {
    TypeId str = type_id(TID_STR), i32 = type_id(TID_I32);
    CHECK(type_eq(type_ref(table, str, false), type_ref(table, str, false)));
    CHECK(!type_eq(type_ref(table, str, false), type_ref(table, str, true)));
    TypeId pair[] = {i32, str};
    TypeId swapped[] = {str, i32};
    CHECK(type_eq(type_tuple(table, pair, 2), type_tuple(table, pair, 2)));
    CHECK(!type_eq(type_tuple(table, pair, 2), type_tuple(table, swapped, 2)));
    CHECK(type_eq(type_tuple(table, NULL, 0), type_id(TID_UNIT)));
    TypeId f = type_fn(table, pair, 2, type_id(TID_BOOL));
    CHECK(type_eq(f, type_fn(table, pair, 2, type_id(TID_BOOL))));
    CHECK(!type_eq(f, type_fn(table, pair, 1, type_id(TID_BOOL))));
}

static void test_growth(TypeTable *table) {
    TypeId t = type_id(TID_INT);
    TypeId first[200];
    for (int i = 0; i < 200; i++) {
        first[i] = t;
        t = type_slice(table, t);
    }
    t = type_id(TID_INT);
    for (int i = 0; i < 200; i++) {
        CHECK(type_eq(first[i], t));   // still found after the table grew
        t = type_slice(table, t);
    }
}

static void test_text(TypeTable *table) {
    TypeId str = type_id(TID_STR);
    TypeId one[] = {type_id(TID_INT)};
    TypeId two[] = {type_id(TID_U8), str};
    CHECK(text_is(table, type_id(TID_INT), "int"));
    CHECK(text_is(table, type_id(TID_I32), "i32"));
    CHECK(text_is(table, type_id(TID_FLOAT), "float"));
    CHECK(text_is(table, type_ref(table, type_slice(table, str), true), "&mut [str]"));
    CHECK(text_is(table, type_tuple(table, one, 1), "(int,)"));
    CHECK(text_is(table, type_tuple(table, two, 2), "(u8, str)"));
    CHECK(text_is(table, type_fn(table, two, 2, type_id(TID_UNIT)), "fn(u8, str)"));
    CHECK(text_is(table, type_fn(table, NULL, 0, type_id(TID_NEVER)), "fn() !"));
}

// Type parameters, substitution into nested types, and nesting depth.
static void test_generics(TypeTable *table) {
    TypeId t = type_param(table, 0, (Symbol){7}), u = type_param(table, 1, (Symbol){8});
    CHECK(type_eq(t, type_param(table, 0, (Symbol){7})));
    CHECK(type_get(table, t)->has_param && !type_get(table, type_id(TID_INT))->has_param);
    TypeId pair[] = {t, u};
    TypeId generic = type_ref(table, type_tuple(table, pair, 2), false);   // &(T, U)
    CHECK(type_get(table, generic)->has_param);
    CHECK(text_is(table, generic, "&(T0, T1)"));
    TypeId args[] = {type_id(TID_STR), type_id(TID_U8)};
    TypeId concrete = type_subst(table, generic, args, 2);
    CHECK(text_is(table, concrete, "&(str, u8)"));
    CHECK(!type_get(table, concrete)->has_param);
    CHECK(type_eq(type_subst(table, concrete, args, 2), concrete));   // nothing to replace
    TypeId adt = type_intern(table, (Ty){.kind = TY_ADT, .data = 3, .arg_count = 1, .args = &t});
    TypeId swapped[] = {u, t};
    CHECK(text_is(table, type_subst(table, type_tuple(table, pair, 2), swapped, 2), "(T1, T0)"));
    CHECK(text_is(table, type_subst(table, adt, args, 2), "adt3<str>"));
    CHECK(type_depth(table, type_id(TID_INT)) == 1);
    CHECK(type_depth(table, concrete) == 3);
}

int main(void) {
    Arena arena;
    arena_init(&arena);
    TypeTable table;
    types_init(&table, &arena);
    test_builtins(&table);
    test_interning(&table);
    test_growth(&table);
    test_text(&table);
    test_generics(&table);
    types_free(&table);
    arena_free(&arena);
    return test_report("types");
}
