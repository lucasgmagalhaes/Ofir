// State shared by the move analysis files (spec 5.1, roadmap 3.1).
//
// After a function is type checked, its body is walked in evaluation order
// with a state per program point: which variables, or parts of them, may
// have been moved out, and which may not be initialized yet. At a join
// (after `if`, `match`, `&&`, loops) a variable is maybe-moved if it is on
// some path. Loops are walked until their entry state no longer changes,
// then once more to report errors.
//
// Stage 0 runs this on the typed AST rather than on AIR: the AST still says
// which uses consume a value (a `const`, a `move` parameter, a return) and
// which only borrow it, and its control flow is structured.

#ifndef OFIR_MOVES_INTERNAL_H
#define OFIR_MOVES_INTERNAL_H

#include "base/arena.h"
#include "sema/check_internal.h"

typedef MovedPart Part;   // a part of a variable moved out (syntax/ast.h)

typedef struct {
    bool dead;           // unreachable: a join ignores it
    uint64_t *moved;     // per local: maybe moved as a whole
    uint64_t *uninit;    // per local: maybe not initialized
    Part *parts;         // parts maybe moved out
    uint32_t part_count, part_cap;
} State;

typedef struct {
    State breaks, continues;
} LoopFrame;

typedef struct {
    Checker *c;
    FnDecl *fn;
    Arena *arena;        // states: the checker's scratch arena, reset per function
    uint32_t words;      // 64-bit words per bitset
    bool report;         // false while a loop's fixpoint is computed
    uint64_t *borrowed;  // per local: borrows a place (borrowed parameter, `for` variable)
    Vec(LoopFrame) loops;
    Vec(Part) all_parts; // every part moved somewhere, for the drops
} Moves;

// How an expression is used: its value read (borrowed), consumed, or, for
// the target of `=`, overwritten.
typedef enum { USE_READ, USE_MOVE, USE_ASSIGN } Use;

// moves.c
State state_new(Moves *m);
State state_copy(Moves *m, const State *s);
void state_join(Moves *m, State *into, const State *other);
void move_error(Moves *m, const char *code, Span span, const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void moves_block(Moves *m, Block *block, State *s, Use tail);
bool moved_part(const State *s, const Part *part);    // overlaps something moved
void move_part(Moves *m, State *s, const Part *part);
void init_part(Moves *m, State *s, const Part *part);  // assigned: no longer moved

// A place rooted at a local: `x`, `x.a.b`, `x.0`. `via` tells what else
// the path goes through, which forbids moving out of it.
typedef enum { VIA_NOTHING, VIA_INDEX, VIA_DEREF, VIA_RAW } Via;

typedef struct {
    bool local;          // rooted at a local (else a temporary or a global)
    Part part;
    Via via;
} Place;

// moves_expr.c
void moves_expr(Moves *m, Expr *e, State *s, Use use);
bool is_copy(Moves *m, TypeId type);
bool is_place(const Expr *e);
Place place_of(Moves *m, Expr *e, State *s);   // walks what the place reads on the way
void use_place(Moves *m, const Place *p, State *s, Span span);   // E0600, E0601

// moves_match.c
void moves_if(Moves *m, Expr *e, State *s, Use use);
void moves_match(Moves *m, Expr *e, State *s, Use use);
void moves_pattern_init(Moves *m, const Pattern *p, State *s, bool borrowed);
// Whether the bindings of `p` take a non-Copy part of the value by move.
bool binds_by_move(Moves *m, const Pattern *p);

static inline bool bit(const uint64_t *set, uint32_t i) {
    return (set[i / 64] >> (i % 64)) & 1;
}

static inline void set_bit(uint64_t *set, uint32_t i, bool on) {
    if (on) {
        set[i / 64] |= 1ull << (i % 64);
    } else {
        set[i / 64] &= ~(1ull << (i % 64));
    }
}

#endif
