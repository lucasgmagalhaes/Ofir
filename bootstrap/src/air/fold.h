// Check elimination: assertions whose condition is known while compiling.

#ifndef OFIR_FOLD_H
#define OFIR_FOLD_H

#include "air/air.h"
#include "sema/types.h"

// Replaces each `assert not c` whose `c` folds to false within its block
// with a goto, then removes the assignments to temporaries no operand reads
// any more (docs/design/compiler-architecture.md, 7).
void air_fold_checks(AirBody *body, const TypeTable *types);

#endif
