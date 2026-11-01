// Move analysis of expressions and places (spec 5.1). Each expression is
// walked as a *read*, which only uses its value (an argument of a borrowed
// parameter, an operand, a condition), or as a *move*, which consumes it
// (a `let`, a `move` parameter, a field of a literal, a return value).
// Moving a place of a non-Copy type marks it moved; any later use of it,
// or of a part of it, is E0600 until it is assigned again.

#include "sema/moves_internal.h"

static StrView name(Moves *m, uint32_t local) {
    return symbol_name(&m->c->sema->session->names, m->fn->locals.items[local].name);
}

static bool is_copy(Moves *m, TypeId type) {
    return type_is_copy(m->c, type);
}

// A place rooted at a local: `x`, `x.a.b`, `x.0`. `via` tells what else
// the path goes through, which forbids moving out of it.
typedef enum { VIA_NOTHING, VIA_INDEX, VIA_DEREF, VIA_RAW } Via;

typedef struct {
    bool local;          // rooted at a local (else a temporary or a global)
    Part part;
    Via via;
} Place;

// The place of `e`, walking the bases it does not reach through fields.
static Place place_of(Moves *m, Expr *e, State *s) {
    if (e->kind == EXPR_IDENT || e->kind == EXPR_SELF) {   // else a constant or a function
        bool local = e->as.ident.res.kind == RES_LOCAL;
        return (Place){.local = local, .part = {.local = local ? e->as.ident.res.index : 0}};
    }
    bool field = e->kind == EXPR_FIELD && e->as.field.res.kind == RES_FIELD;
    if (field || e->kind == EXPR_TUPLE_FIELD) {
        Place base = place_of(m, field ? e->as.field.base : e->as.tuple_field.base, s);
        if (base.local && base.part.len < PART_DEPTH) {
            base.part.fields[base.part.len++] = field ? e->as.field.res.index
                                                      : e->as.tuple_field.index;
        }
        return base;
    }
    if (e->kind == EXPR_INDEX) {
        Place base = place_of(m, e->as.index.base, s);
        moves_expr(m, e->as.index.index, s, USE_READ);
        base.via = VIA_INDEX;
        return base;
    }
    if (e->kind == EXPR_UNARY && e->as.unary.op == OP_DEREF) {
        moves_expr(m, e->as.unary.operand, s, USE_READ);
        const Ty *ty = type_get(m->c->types, e->as.unary.operand->type);
        return (Place){.via = ty->kind == TY_PTR ? VIA_RAW : VIA_DEREF};
    }
    moves_expr(m, e, s, USE_READ);   // a temporary: `make().x`
    return (Place){0};
}

static void use_place(Moves *m, const Place *p, State *s, Span span) {
    if (!p->local) {
        return;
    }
    if (bit(s->uninit, p->part.local)) {
        move_error(m, "E0601", span, "use of possibly uninitialized variable `" SV_FMT "`",
                   SV_ARG(name(m, p->part.local)));
    } else if (moved_part(s, &p->part)) {
        move_error(m, "E0600", span, "use of moved value `" SV_FMT "`: it was moved before; "
                   "clone it, or move a copy", SV_ARG(name(m, p->part.local)));
    }
}

// Moving out of `p`: only out of a place the function owns entirely.
static void move_place(Moves *m, const Place *p, State *s, Span span) {
    if (p->via == VIA_INDEX || p->via == VIA_DEREF) {
        move_error(m, "E0602", span, "cannot move out of %s: use `remove`, `swapRemove`, "
                   "`mem.replace` or `.clone()`", p->via == VIA_INDEX ? "an element of a "
                   "collection" : "a reference");
        return;
    }
    if (!p->local || p->via == VIA_RAW) {
        return;   // a temporary, or a raw pointer read (unsafe)
    }
    if (bit(m->borrowed, p->part.local)) {
        move_error(m, "E0602", span, "cannot move out of `" SV_FMT "`, which is borrowed: "
                   "declare the parameter `move`, or call `.clone()`",
                   SV_ARG(name(m, p->part.local)));
        return;
    }
    move_part(m, s, &p->part);
}

static bool is_place(const Expr *e) {
    return e->kind == EXPR_IDENT || e->kind == EXPR_SELF || e->kind == EXPR_TUPLE_FIELD ||
           e->kind == EXPR_INDEX || (e->kind == EXPR_FIELD && e->as.field.res.kind == RES_FIELD) ||
           (e->kind == EXPR_UNARY && e->as.unary.op == OP_DEREF);
}

// `p = value`: `p` holds a value again. A field of a moved struct cannot be
// assigned: the struct as a whole is gone.
static void assign_place(Moves *m, Expr *e, const Place *p, State *s, Span span) {
    if (!p->local) {
        return;
    }
    // The old value of a part that may have been moved out is not dropped.
    e->moves = e->moves || moved_part(s, &p->part);
    if (p->part.len > 0 && bit(s->moved, p->part.local)) {
        use_place(m, &(Place){.local = true, .part = {.local = p->part.local}}, s, span);
    }
    init_part(m, s, &p->part);
}

static void place_expr(Moves *m, Expr *e, State *s, Use use) {
    Place p = place_of(m, e, s);
    if (use == USE_ASSIGN) {   // the target of `=`: overwritten, not read
        assign_place(m, e, &p, s, e->span);
        return;
    }
    use_place(m, &p, s, e->span);
    if (use == USE_MOVE && !is_copy(m, e->type)) {
        move_place(m, &p, s, e->span);
        e->moves = e->moves || (p.local && p.via == VIA_NOTHING);   // for the drop flags
    }
}

// ---------------------------------------------------------------------------
// Calls

static void each_arg(Moves *m, ArgList args, const FnDecl *fn, Use use, State *s) {
    for (uint32_t i = 0; i < args.len; i++) {
        Use arg = use;
        if (fn && i < fn->params.len) {
            arg = fn->params.items[i].mode == MODE_MOVE ? USE_MOVE : USE_READ;
        }
        moves_expr(m, args.items[i].value, s, arg);
    }
}

// A call consumes the arguments of `move` parameters (and of constructors
// and variants) and only reads the others.
static void call(Moves *m, Expr *e, State *s) {
    Expr *callee = e->as.call.callee;
    ArgList args = e->as.call.args;
    if (callee->kind == EXPR_FIELD) {
        Res res = callee->as.field.res;
        if (res.kind == RES_DEF) {   // a method, or an associated function
            FnDecl *fn = sema_def(m->c->sema, (DefId){res.index})->fn;
            if (fn->has_self) {
                Use self = fn->self_mode == MODE_MOVE ? USE_MOVE : USE_READ;
                moves_expr(m, callee->as.field.base, s, self);
            }
            each_arg(m, args, fn, USE_READ, s);
            return;
        }
        if (res.kind != RES_VARIANT) {   // the built-in methods read
            moves_expr(m, callee->as.field.base, s, USE_READ);
        }
        each_arg(m, args, NULL, res.kind == RES_VARIANT ? USE_MOVE : USE_READ, s);
        return;
    }
    Res res = callee->kind == EXPR_GENERIC ? callee->as.generic.res : callee->as.ident.res;
    const Def *def = res.kind == RES_DEF ? sema_def(m->c->sema, (DefId){res.index}) : NULL;
    if (!def) {
        moves_expr(m, callee, s, USE_READ);
    }
    bool build = def && (def->kind == DEF_STRUCT || def->kind == DEF_VARIANT);
    each_arg(m, args, def && def->kind == DEF_FN ? def->fn : NULL,
             build ? USE_MOVE : USE_READ, s);
}

// ---------------------------------------------------------------------------
// Patterns, `if` and `match`

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
        set_bit(s->moved, local, false);
        set_bit(s->uninit, local, false);
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
    if (!is_place(e)) {
        moves_expr(m, e, s, USE_MOVE);
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

static void if_expr(Moves *m, Expr *e, State *s, Use use) {
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

static void match_expr(Moves *m, Expr *e, State *s, Use use) {
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

// ---------------------------------------------------------------------------

static void each(Moves *m, ExprList list, State *s, Use use) {
    for (uint32_t i = 0; i < list.len; i++) {
        moves_expr(m, list.items[i], s, use);
    }
}

// `a && b`, `a || b`, `a ?? b`: `b` is evaluated on some paths only.
static void conditional(Moves *m, Expr *e, State *s, Use lhs, Use rhs) {
    moves_expr(m, e->as.binary.lhs, s, lhs);
    State other = state_copy(m, s);
    moves_expr(m, e->as.binary.rhs, &other, rhs);
    state_join(m, s, &other);
}

static void binary(Moves *m, Expr *e, State *s, Use use) {
    Op op = e->as.binary.op;
    if (op == OP_LOGICAL_AND || op == OP_LOGICAL_OR) {
        conditional(m, e, s, USE_READ, USE_READ);
    } else if (op == OP_COALESCE) {
        conditional(m, e, s, USE_MOVE, use);
    } else {
        moves_expr(m, e->as.binary.lhs, s, USE_READ);
        moves_expr(m, e->as.binary.rhs, s, USE_READ);
    }
}

static void unary(Moves *m, Expr *e, State *s) {
    if (e->as.unary.op == OP_REF || e->as.unary.op == OP_REF_MUT) {   // a borrow
        Expr *operand = e->as.unary.operand;
        if (is_place(operand)) {
            Place p = place_of(m, operand, s);
            use_place(m, &p, s, operand->span);
        } else {
            moves_expr(m, operand, s, USE_READ);
        }
        return;
    }
    moves_expr(m, e->as.unary.operand, s, USE_READ);
}

static void literal(Moves *m, Expr *e, State *s) {
    for (uint32_t i = 0; i < e->as.struct_.fields.len; i++) {
        moves_expr(m, e->as.struct_.fields.items[i].value, s, USE_MOVE);
    }
    if (e->as.struct_.base) {
        moves_expr(m, e->as.struct_.base, s, USE_MOVE);
    }
}

// check-sizes: table
static void walk(Moves *m, Expr *e, State *s, Use use) {
    switch (e->kind) {
    case EXPR_INTERP: each(m, e->as.parts, s, USE_READ); break;
    case EXPR_UNARY: unary(m, e, s); break;
    case EXPR_BINARY: binary(m, e, s, use); break;
    case EXPR_CAST: moves_expr(m, e->as.cast.value, s, USE_READ); break;
    case EXPR_RANGE:
        if (e->as.range.lo) {
            moves_expr(m, e->as.range.lo, s, USE_READ);
        }
        if (e->as.range.hi) {
            moves_expr(m, e->as.range.hi, s, USE_READ);
        }
        break;
    case EXPR_CALL: call(m, e, s); break;
    case EXPR_TRY: moves_expr(m, e->as.inner, s, USE_MOVE); break;
    case EXPR_STRUCT: literal(m, e, s); break;
    case EXPR_ARRAY: case EXPR_TUPLE: each(m, e->as.elems, s, USE_MOVE); break;
    case EXPR_ARRAY_REPEAT:
        moves_expr(m, e->as.repeat.value, s, USE_READ);
        moves_expr(m, e->as.repeat.count, s, USE_READ);
        break;
    case EXPR_VARIANT:
        each_arg(m, e->as.variant.args, NULL, USE_MOVE, s);
        break;
    case EXPR_IF: if_expr(m, e, s, use); break;
    case EXPR_MATCH: match_expr(m, e, s, use); break;
    case EXPR_BLOCK: case EXPR_UNSAFE: moves_block(m, e->as.block, s, use); break;
    default: break;   // literals, names of types and functions, constants
    }
}

void moves_expr(Moves *m, Expr *e, State *s, Use use) {
    if (s->dead) {
        return;   // unreachable code moves nothing
    }
    if (is_place(e)) {
        place_expr(m, e, s, use);
    } else {
        walk(m, e, s, use);
    }
    if (e->type.id == TID_NEVER) {
        s->dead = true;
    }
}
