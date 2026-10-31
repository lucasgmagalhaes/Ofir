// Code generation: AIR → textual LLVM IR (ADR 0001), accepted by LLVM 17 and
// later. Stage 0 never emits `noalias` (it has no borrow checker).

#ifndef ASTRA_LLVM_H
#define ASTRA_LLVM_H

#include "air/air.h"
#include "sema/sema.h"

#include <stdio.h>

void llvm_generate(Sema *sema, const AirProgram *program, FILE *out);

#endif
