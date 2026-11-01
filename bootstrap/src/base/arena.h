// Arena allocator: allocations are released all at once.
//
// Nearly all compiler data lives for the whole compilation, so stage 0
// allocates it from arenas instead of tracking individual lifetimes.

#ifndef ASTRA_ARENA_H
#define ASTRA_ARENA_H

#include "base/common.h"

typedef struct ArenaChunk ArenaChunk;

typedef struct {
    ArenaChunk *head;      // chunk currently allocated from
    size_t total;          // bytes handed out, for statistics
} Arena;

void arena_init(Arena *arena);
void arena_free(Arena *arena);
// Releases every allocation but keeps one chunk: for scratch memory reused often.
void arena_reset(Arena *arena);

// Returns `size` bytes aligned to `align` (a power of two), zero-initialized.
void *arena_alloc(Arena *arena, size_t size, size_t align);

// Like arena_alloc, without zeroing: for memory that is overwritten at once.
void *arena_alloc_uninit(Arena *arena, size_t size, size_t align);

// Copies `len` bytes and appends a terminating zero byte.
char *arena_strndup(Arena *arena, const char *src, size_t len);

#define ARENA_NEW(arena, T) ((T *)arena_alloc((arena), sizeof(T), _Alignof(T)))
#define ARENA_ARRAY(arena, T, n) ((T *)arena_alloc((arena), sizeof(T) * (n), _Alignof(T)))

#endif
