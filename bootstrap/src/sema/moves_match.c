// Move analysis of patterns, `if let` and `match` (spec 5.1, 5.5): a
// matched owned value is moved into the bindings that take parts of it by
// move; the bindings of a matched borrowed place borrow.

#include "sema/moves_internal.h"

static bool any_by_move(Moves *m, PatternList list) {
    for (uint32_t i = 0; i < list.len; i++) {
        if (binds_by_move(m, list.items[i])) {
            return true;
        }
    }
    return false;
}

bool binds_by_move(Moves *m, const Pattern *p) {
    switch (p->kind) {
    case PAT_BINDING: return !is_copy(m, p->type);
    case PAT_VARIANT: return any_by_move(m, p->as.variant.args);
    case PAT_TUPLE: return any_by_move(m, p->as.tuple);
    case PAT_TUPLE_STRUCT: return any_by_move(m, p->as.tuple_struct.args);
    case PAT_OR: return any_by_move(m, p->as.alternatives);
    case PAT_STRUCT:
        for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
            if (binds_by_move(m, p->as.struct_.fields.items[i].pattern)) {
                return true;
            }
        }
        return false;
    default: return false;
    }
}

static void init_list(Moves *m, PatternList list, State *s, bool borrowed) {
    for (uint32_t i = 0; i < list.len; i++) {
        moves_pattern_init(m, list.items[i], s, borrowed);
    }
}

// The bindings of `p` hold values; `borrowed` ones borrow the matched place.
void moves_pattern_init(Moves *m, const Pattern *p, State *s, bool borrowed) {
    switch (p->kind) {
    case PAT_BINDING: {
        uint32_t local = p->as.binding.local.id;
        init_part(m, s, &(Part){.local = local});   // also the parts moved out before
        set_bit(m->borrowed, local, borrowed);
        break;
    }
    case PAT_VARIANT: init_list(m, p->as.variant.args, s, borrowed); break;
    case PAT_TUPLE: init_list(m, p->as.tuple, s, borrowed); break;
    case PAT_TUPLE_STRUCT: init_list(m, p->as.tuple_struct.args, s, borrowed); break;
    case PAT_OR: init_list(m, p->as.alternatives, s, borrowed); break;
    case PAT_STRUCT:
        for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
            moves_pattern_init(m, p->as.struct_.fields.items[i].pattern, s, borrowed);
        }
        break;
    default: break;
    }
}

// The value matched by `match` or `if let`: an owned place whose parts are
// bound by move is moved; a borrowed place lends its parts to the
// bindings; a temporary is consumed. Returns whether the bindings borrow.
static bool scrutinee(Moves *m, Expr *e, State *s, bool by_move) {
    if (!is_place(e)) {   // a temporary: consumed if its parts are bound by move
        moves_expr(m, e, s, by_move ? USE_MOVE : USE_READ);
        return false;
    }
    Place p = place_of(m, e, s);
    use_place(m, &p, s, e->span);
    bool owned = p.local && p.via == VIA_NOTHING && !bit(m->borrowed, p.part.local);
    if (by_move && owned) {
        move_part(m, s, &p.part);
        e->moves = true;
    }
    return !owned && p.via != VIA_RAW;
}

void moves_if(Moves *m, Expr *e, State *s, Use use) {
    bool borrowed = false;
    Pattern *pattern = e->as.if_.let_pattern;
    if (pattern) {
        borrowed = scrutinee(m, e->as.if_.cond, s, binds_by_move(m, pattern));
    } else {
        moves_expr(m, e->as.if_.cond, s, USE_READ);
    }
    State other = state_copy(m, s);
    if (pattern) {
        moves_pattern_init(m, pattern, s, borrowed);
    }
    moves_block(m, e->as.if_.then, s, use);
    if (e->as.if_.else_) {
        moves_expr(m, e->as.if_.else_, &other, use);
    }
    state_join(m, s, &other);
}

void moves_match(Moves *m, Expr *e, State *s, Use use) {
    bool by_move = false;
    for (uint32_t i = 0; i < e->as.match.arms.len; i++) {
        by_move = by_move || binds_by_move(m, e->as.match.arms.items[i].pattern);
    }
    bool borrowed = scrutinee(m, e->as.match.scrutinee, s, by_move);
    State result = {.dead = true};
    for (uint32_t i = 0; i < e->as.match.arms.len; i++) {
        MatchArm *arm = &e->as.match.arms.items[i];
        State a = state_copy(m, s);
        moves_pattern_init(m, arm->pattern, &a, borrowed);
        if (arm->guard) {
            moves_expr(m, arm->guard, &a, USE_READ);
        }
        moves_expr(m, arm->body, &a, use);
        state_join(m, &result, &a);
    }
    *s = result;
}

