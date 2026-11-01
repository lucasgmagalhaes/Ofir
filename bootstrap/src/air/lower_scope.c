// Drop scopes and drop flags (spec 5.2): which locals own a value to drop,
// and the drops at the end of their block and at the jumps that leave it.

#include "air/lower_internal.h"

void drop_scope_enter(Lowerer *l) {
    VEC_PUSH(l->scopes, (uint32_t)l->owned.len);
}

static TypeId local_type(Lowerer *l, LocalId local) {
    return concrete_type(l, l->body->locals.data[local.id].type);
}

// `if flag { drop(local) }`.
static void drop_if_set(Lowerer *l, LocalId local) {
    LocalId flag = l->flags[local.id];
    BlockId drop = air_new_block(l->body), next = air_new_block(l->body);
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = op_copy(flag, type_id(TID_BOOL)),
                              .target = drop, .other = next});
    switch_to(l, drop);
    emit_drop(l, (Place){.local = local, .path = {.deref = l->body->locals.data[local.id].by_ref}},
              local_type(l, local));
    emit_goto(l, next);
    switch_to(l, next);
}

// Drops the locals owned by the scopes from `first` on, the latest first,
// without closing the scopes: for the jumps that leave them.
void drop_scopes(Lowerer *l, uint32_t first) {
    if (first >= l->scopes.len) {
        return;
    }
    for (size_t i = l->owned.len; i > l->scopes.data[first]; i--) {
        drop_if_set(l, l->owned.data[i - 1]);
    }
}

void drop_scope_leave(Lowerer *l) {
    drop_scopes(l, (uint32_t)l->scopes.len - 1);
    l->owned.len = VEC_LAST(l->scopes);
    l->scopes.len--;
}

bool drop_scope_owns(Lowerer *l) {
    return l->scopes.len > 0 && l->owned.len > VEC_LAST(l->scopes);
}

static void set_flag(Lowerer *l, LocalId local, bool value) {
    emit_assign(l, l->flags[local.id], (Rvalue){.kind = RV_USE, .a = op_bool(value)});
}

void own_local(Lowerer *l, LocalId local, bool initialized) {
    if (local.id >= l->flag_count || l->body->def.id == 0 || !needs_drop(l, local_type(l, local))) {
        return;
    }
    if (local.id > 0 && l->fn_locals && l->fn_locals[local.id - 1].borrows) {
        return;   // a `for` variable or a binding of a borrowed place
    }
    if (l->flags[local.id].id == 0) {
        l->flags[local.id] = new_temp(l, type_id(TID_BOOL));
    }
    set_flag(l, local, initialized);
    VEC_PUSH(l->owned, local);
}

void drop_moved(Lowerer *l, LocalId local) {
    if (local.id < l->flag_count && l->flags[local.id].id != 0) {
        set_flag(l, local, false);
    }
}

// Before `local = value`: the old value is dropped; after it, the flag is
// set (assign_done).
void drop_before_assign(Lowerer *l, LocalId local) {
    if (local.id < l->flag_count && l->flags[local.id].id != 0) {
        drop_if_set(l, local);
    }
}

void assign_done(Lowerer *l, LocalId local) {
    if (local.id < l->flag_count && l->flags[local.id].id != 0) {
        set_flag(l, local, true);
    }
}

static void own_list(Lowerer *l, PatternList list) {
    for (uint32_t i = 0; i < list.len; i++) {
        own_bindings(l, list.items[i]);
    }
}

void own_bindings(Lowerer *l, const Pattern *p) {
    switch (p->kind) {
    case PAT_BINDING: own_local(l, air_local_of(p->as.binding.local), true); break;
    case PAT_VARIANT: own_list(l, p->as.variant.args); break;
    case PAT_TUPLE: own_list(l, p->as.tuple); break;
    case PAT_TUPLE_STRUCT: own_list(l, p->as.tuple_struct.args); break;
    case PAT_OR: own_bindings(l, p->as.alternatives.items[0]); break;   // same locals
    case PAT_STRUCT:
        for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
            own_bindings(l, p->as.struct_.fields.items[i].pattern);
        }
        break;
    default: break;
    }
}

// The local at the root of a moved place: its flag is cleared.
void note_moved(Lowerer *l, const Expr *e) {
    while (e->kind == EXPR_FIELD || e->kind == EXPR_TUPLE_FIELD) {
        e = e->kind == EXPR_FIELD ? e->as.field.base : e->as.tuple_field.base;
    }
    if ((e->kind == EXPR_IDENT || e->kind == EXPR_SELF) && e->as.ident.res.kind == RES_LOCAL) {
        drop_moved(l, air_local_of((LocalId){e->as.ident.res.index}));
    }
}
