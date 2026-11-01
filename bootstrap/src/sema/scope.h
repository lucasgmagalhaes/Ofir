// Scopes: the names visible at a point of the program.
//
// Every table is indexed by symbol id, so a lookup is one array access. The
// module table holds the prelude and the top-level declarations. Locals use
// a binding stack: declaring a local saves the previous binding of its name,
// and leaving a block restores it.

#ifndef ASTRA_SCOPE_H
#define ASTRA_SCOPE_H

#include "base/intern.h"
#include "base/vec.h"
#include "sema/ids.h"

typedef struct {
    Symbol name;
    uint32_t previous;   // the binding it hides: local index + 1, or 0
} Shadowed;

typedef struct {
    uint32_t symbol_count;   // size of the tables below
    uint32_t **tables;       // per module: symbol id -> DefId (0 = none)
    uint32_t table_count;
    uint32_t *module;        // the current module's table
    uint32_t *local;         // symbol id -> local index + 1 (0 = none)
    Vec(uint32_t) depth;     // local index -> block depth where it was declared
    Vec(Shadowed) saved;     // undo log of local declarations
    Vec(size_t) marks;       // saved.len at the start of each open block
} Scope;

void scope_init(Scope *scope, uint32_t symbol_count, uint32_t module_count);
void scope_free(Scope *scope);

void scope_use_module(Scope *scope, uint32_t index);

// Lookups in the current module's table, or in module `index`'s.
DefId scope_module(const Scope *scope, Symbol name);
DefId scope_module_in(const Scope *scope, uint32_t index, Symbol name);
void scope_set_module(Scope *scope, Symbol name, DefId def);

void scope_enter(Scope *scope);
void scope_leave(Scope *scope);

// Starts a function body: forgets every local.
void scope_reset_locals(Scope *scope);

// Returns the local bound to `name`, if any.
bool scope_local(const Scope *scope, Symbol name, LocalId *out);

// Whether `name` is already a local of the innermost block.
bool scope_in_current_block(const Scope *scope, Symbol name);

// Binds `name` to local `id` in the innermost block. Ids are dense and
// increase within one function.
void scope_declare(Scope *scope, Symbol name, LocalId id);

#endif
