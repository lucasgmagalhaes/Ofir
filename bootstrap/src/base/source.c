#include "base/source.h"

#include <stdio.h>
#include <stdlib.h>

void source_map_init(SourceMap *map, Arena *arena) {
    map->arena = arena;
    map->files = NULL;
    map->count = 0;
    map->cap = 0;
}

void source_map_free(SourceMap *map) {
    free(map->files);
    map->files = NULL;
    map->count = map->cap = 0;
}

FileId source_map_add(SourceMap *map, StrView path, StrView text) {
    if (text.len > UINT32_MAX - 1) {
        fatal("source file too large: " SV_FMT, SV_ARG(path));
    }
    if (map->count == map->cap) {
        map->cap = map->cap ? map->cap * 2 : 16;
        map->files = xrealloc(map->files, sizeof(SourceFile) * map->cap);
    }

    SourceFile *file = &map->files[map->count];
    file->path = (StrView){arena_strndup(map->arena, path.ptr, path.len), path.len};
    file->text = (StrView){arena_strndup(map->arena, text.ptr, text.len), text.len};

    uint32_t lines = 1;
    for (size_t i = 0; i < text.len; i++) {
        lines += text.ptr[i] == '\n';
    }
    file->line_starts = ARENA_ARRAY(map->arena, uint32_t, lines);
    file->line_starts[0] = 0;
    uint32_t line = 1;
    for (size_t i = 0; i < text.len; i++) {
        if (text.ptr[i] == '\n') {
            file->line_starts[line++] = (uint32_t)i + 1;
        }
    }
    file->line_count = lines;
    return (FileId){map->count++};
}

bool source_map_load(SourceMap *map, const char *path, FileId *out) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    StrBuf buf = {0};
    char chunk[16384];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) {
        strbuf_append(&buf, (StrView){chunk, n});
    }
    bool ok = !ferror(f);
    fclose(f);
    if (ok) {
        *out = source_map_add(map, sv_from_cstr(path), strbuf_view(&buf));
    }
    strbuf_free(&buf);
    return ok;
}

const SourceFile *source_file(const SourceMap *map, FileId id) {
    return &map->files[id.id];
}

LineCol source_line_col(const SourceMap *map, FileId id, uint32_t offset) {
    const SourceFile *file = source_file(map, id);
    // Binary search for the last line starting at or before `offset`.
    uint32_t lo = 0, hi = file->line_count;
    while (hi - lo > 1) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (file->line_starts[mid] <= offset) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return (LineCol){lo + 1, offset - file->line_starts[lo] + 1};
}

StrView source_line_text(const SourceMap *map, FileId id, uint32_t line) {
    const SourceFile *file = source_file(map, id);
    if (line == 0 || line > file->line_count) {
        return (StrView){"", 0};
    }
    uint32_t start = file->line_starts[line - 1];
    uint32_t end = line < file->line_count ? file->line_starts[line] : (uint32_t)file->text.len;
    while (end > start && (file->text.ptr[end - 1] == '\n' || file->text.ptr[end - 1] == '\r')) {
        end--;
    }
    return (StrView){file->text.ptr + start, end - start};
}

Span span_join(Span a, Span b) {
    return (Span){a.file, a.start < b.start ? a.start : b.start, a.end > b.end ? a.end : b.end};
}
