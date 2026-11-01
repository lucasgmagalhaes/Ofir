#include "base/arena.h"

#include <stdlib.h>
#include <string.h>

#define ARENA_CHUNK_SIZE (64 * 1024)

struct ArenaChunk {
    ArenaChunk *prev;
    size_t used;
    size_t capacity;
    _Alignas(max_align_t) unsigned char data[];
};

void arena_init(Arena *arena) {
    arena->head = NULL;
    arena->total = 0;
}

void arena_free(Arena *arena) {
    ArenaChunk *chunk = arena->head;
    while (chunk) {
        ArenaChunk *prev = chunk->prev;
        free(chunk);
        chunk = prev;
    }
    arena_init(arena);
}

void arena_reset(Arena *arena) {
    ArenaChunk *chunk = arena->head;
    while (chunk && chunk->prev) {   // keep the oldest chunk, for reuse
        ArenaChunk *prev = chunk->prev;
        free(chunk);
        chunk = prev;
    }
    if (chunk) {
        chunk->used = 0;
    }
    arena->head = chunk;
    arena->total = 0;
}

static ArenaChunk *new_chunk(ArenaChunk *prev, size_t min_size) {
    size_t capacity = min_size > ARENA_CHUNK_SIZE ? min_size : ARENA_CHUNK_SIZE;
    ArenaChunk *chunk = xmalloc(sizeof(ArenaChunk) + capacity);
    chunk->prev = prev;
    chunk->used = 0;
    chunk->capacity = capacity;
    return chunk;
}

void *arena_alloc_uninit(Arena *arena, size_t size, size_t align) {
    ArenaChunk *chunk = arena->head;
    size_t offset = 0;
    if (chunk) {
        offset = (chunk->used + align - 1) & ~(align - 1);
    }
    if (!chunk || offset + size > chunk->capacity) {
        // Large requests get a dedicated chunk; the current one stays the
        // head only if it still has room for small allocations.
        chunk = new_chunk(arena->head, size + align);
        arena->head = chunk;
        offset = 0;
    }
    void *ptr = chunk->data + offset;
    chunk->used = offset + size;
    arena->total += size;
    return ptr;
}

void *arena_alloc(Arena *arena, size_t size, size_t align) {
    void *ptr = arena_alloc_uninit(arena, size, align);
    memset(ptr, 0, size);
    return ptr;
}

char *arena_strndup(Arena *arena, const char *src, size_t len) {
    char *dst = arena_alloc_uninit(arena, len + 1, 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}
