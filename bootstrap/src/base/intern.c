#include "base/intern.h"

#include <stdlib.h>

void interner_init(Interner *interner, Arena *arena) {
    interner->arena = arena;
    interner->names_cap = 256;
    interner->names = xmalloc(sizeof(StrView) * interner->names_cap);
    interner->names[0] = (StrView){"", 0};
    interner->count = 1;
    interner->slots_cap = 512;
    interner->slots = xcalloc(interner->slots_cap, sizeof(uint32_t));
}

void interner_free(Interner *interner) {
    free(interner->names);
    free(interner->slots);
}

static void grow_slots(Interner *interner) {
    uint32_t cap = interner->slots_cap * 2;
    uint32_t *slots = xcalloc(cap, sizeof(uint32_t));
    for (uint32_t id = 1; id < interner->count; id++) {
        uint32_t i = (uint32_t)sv_hash(interner->names[id]) & (cap - 1);
        while (slots[i]) {
            i = (i + 1) & (cap - 1);
        }
        slots[i] = id;
    }
    free(interner->slots);
    interner->slots = slots;
    interner->slots_cap = cap;
}

Symbol intern(Interner *interner, StrView name) {
    uint32_t mask = interner->slots_cap - 1;
    uint32_t i = (uint32_t)sv_hash(name) & mask;
    while (interner->slots[i]) {
        uint32_t id = interner->slots[i];
        if (sv_eq(interner->names[id], name)) {
            return (Symbol){id};
        }
        i = (i + 1) & mask;
    }

    if (interner->count == interner->names_cap) {
        interner->names_cap *= 2;
        interner->names = xrealloc(interner->names, sizeof(StrView) * interner->names_cap);
    }
    uint32_t id = interner->count++;
    interner->names[id] = (StrView){arena_strndup(interner->arena, name.ptr, name.len), name.len};
    interner->slots[i] = id;
    if (interner->count * 2 > interner->slots_cap) {   // keep load factor below 1/2
        grow_slots(interner);
    }
    return (Symbol){id};
}

StrView symbol_name(const Interner *interner, Symbol symbol) {
    return interner->names[symbol.id];
}
