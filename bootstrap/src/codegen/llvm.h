// Code generation: AIR → textual LLVM IR (ADR 0001), accepted by LLVM 17 and
// later. Stage 0 never emits `noalias` (it has no borrow checker).

#ifndef OFIR_LLVM_H
#define OFIR_LLVM_H

#include "air/air.h"
#include "sema/sema.h"

#include <stdio.h>

// The module is written one function at a time: llvm_begin, then
// llvm_function for each body, then llvm_end, which writes the declarations
// and the constants and frees the generator.
typedef struct Gen Gen;

Gen *llvm_begin(Sema *sema, FILE *out);
void llvm_function(Gen *g, const AirBody *body);
void llvm_end(Gen *g);

#endif
