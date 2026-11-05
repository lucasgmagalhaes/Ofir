// "Did you mean" suggestions (docs/design/toolchain.md, Suggestions): the
// candidate closest to an unknown name by edit distance, if it is within a
// third of the name's length. Between equally close candidates the one with
// the smaller symbol id (interned first) wins, so the order in which they
// are offered does not matter.

#ifndef OFIR_SUGGEST_H
#define OFIR_SUGGEST_H

#include "base/intern.h"
#include "diag/diag.h"
#include "sema/sema.h"

typedef struct {
    StrView name;        // the unknown name
    Symbol best;         // the closest candidate so far (id 0 = none)
    StrView best_name;
    size_t distance;     // its edit distance from `name`
} Suggestion;

Suggestion suggestion_start(StrView name);
void suggestion_offer(Suggestion *s, Symbol candidate, StrView text);

// Adds `= help: did you mean `x`?` to the diagnostic reported last.
void suggestion_report(const Suggestion *s, Diagnostics *diags);

// Where suggest_name looks for candidates.
typedef enum {
    SUGGEST_VALUE,   // the locals in scope and the current module's table
    SUGGEST_ITEM,    // the current module's table only (after resolution)
    SUGGEST_TYPE,    // the types of the current module's table
} SuggestScope;

// The help line for an unknown name, after its E0200.
void suggest_name(Sema *sema, Symbol name, SuggestScope where);

// The help line for an unknown member of module `index`: its public
// declarations.
void suggest_member(Sema *sema, uint32_t index, Symbol name);

#endif
