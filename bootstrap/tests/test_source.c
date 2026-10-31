#include "base/source.h"
#include "test.h"

static void test_line_col(void) {
    Arena arena;
    arena_init(&arena);
    SourceMap map;
    source_map_init(&map, &arena);

    FileId id = source_map_add(&map, SV("a.at"), SV("fn main() {\n    x\r\n}\n"));
    LineCol lc = source_line_col(&map, id, 0);
    CHECK(lc.line == 1 && lc.column == 1);
    lc = source_line_col(&map, id, 16);              // `x`
    CHECK(lc.line == 2 && lc.column == 5);
    lc = source_line_col(&map, id, 19);              // `}`
    CHECK(lc.line == 3 && lc.column == 1);
    CHECK(sv_eq(source_line_text(&map, id, 2), SV("    x")));
    CHECK(sv_eq(source_line_text(&map, id, 3), SV("}")));
    CHECK(source_line_text(&map, id, 99).len == 0);

    FileId empty = source_map_add(&map, SV("b.at"), SV(""));
    lc = source_line_col(&map, empty, 0);
    CHECK(lc.line == 1 && lc.column == 1);
    CHECK(empty.id == 1);

    source_map_free(&map);
    arena_free(&arena);
}

static void test_load_missing_file(void) {
    Arena arena;
    arena_init(&arena);
    SourceMap map;
    source_map_init(&map, &arena);
    FileId id = {99};
    CHECK(!source_map_load(&map, "/nonexistent/file.at", &id));
    CHECK(id.id == 99);
    source_map_free(&map);
    arena_free(&arena);
}

int main(void) {
    test_line_col();
    test_load_missing_file();
    return test_report("source");
}
