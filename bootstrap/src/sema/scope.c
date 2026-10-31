#include "sema/scope.h"

void scope_init(Scope *scope, uint32_t symbol_count) {
    *scope = (Scope){.symbol_count = symbol_count};
    scope->module = xcalloc(symbol_count, sizeof(uint32_t));
    scope->local = xcalloc(symbol_count, sizeof(uint32_t));
}

void scope_free(Scope *scope) {
    free(scope->module);
    free(scope->local);
    VEC_FREE(scope->depth);
    VEC_FREE(scope->saved);
    VEC_FREE(scope->marks);
}

DefId scope_module(const Scope *scope, Symbol name) {
    return (DefId){name.id < scope->symbol_count ? scope->module[name.id] : 0};
}

void scope_set_module(Scope *scope, Symbol name, DefId def) {
    scope->module[name.id] = def.id;
}

void scope_enter(Scope *scope) {
    VEC_PUSH(scope->marks, scope->saved.len);
}

void scope_leave(Scope *scope) {
    size_t mark = VEC_POP(scope->marks);
    while (scope->saved.len > mark) {
        Shadowed s = VEC_POP(scope->saved);
        scope->local[s.name.id] = s.previous;
    }
}

void scope_reset_locals(Scope *scope) {
    if (scope->marks.len != 0) {
        fatal("scope_reset_locals: a block is still open");
    }
    scope->depth.len = 0;
}

bool scope_local(const Scope *scope, Symbol name, LocalId *out) {
    uint32_t bound = name.id < scope->symbol_count ? scope->local[name.id] : 0;
    if (bound == 0) {
        return false;
    }
    *out = (LocalId){bound - 1};
    return true;
}

bool scope_in_current_block(const Scope *scope, Symbol name) {
    LocalId id;
    return scope_local(scope, name, &id) && scope->depth.data[id.id] == scope->marks.len;
}

void scope_declare(Scope *scope, Symbol name, LocalId id) {
    Shadowed s = {.name = name, .previous = scope->local[name.id]};
    VEC_PUSH(scope->saved, s);
    scope->local[name.id] = id.id + 1;
    VEC_PUSH(scope->depth, (uint32_t)scope->marks.len);
}
