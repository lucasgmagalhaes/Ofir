// Identifier interning: each distinct name is stored once and identified by
// a small integer, so names compare with `==`.

#ifndef ASTRA_INTERN_H
#define ASTRA_INTERN_H

#include "base/arena.h"
#include "base/str.h"

typedef struct {
    uint32_t id;    // 0 is reserved for "no symbol"
} Symbol;

typedef struct {
    Arena *arena;          // owns the copied names
    StrView *names;        // names[id]; names[0] is empty
    uint32_t count;        // number of entries in names, including entry 0
    uint32_t names_cap;
    uint32_t *slots;       // open addressing table of ids (0 = empty)
    uint32_t slots_cap;    // a power of two
} Interner;

void interner_init(Interner *interner, Arena *arena);
void interner_free(Interner *interner);
Symbol intern(Interner *interner, StrView name);
StrView symbol_name(const Interner *interner, Symbol symbol);

static inline bool symbol_eq(Symbol a, Symbol b) {
    return a.id == b.id;
}

#endif
