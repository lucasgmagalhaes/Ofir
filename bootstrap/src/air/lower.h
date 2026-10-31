// Lowering: the typed AST of a module → AIR (one body per function).

#ifndef ASTRA_LOWER_H
#define ASTRA_LOWER_H

#include "air/air.h"
#include "sema/sema.h"

// Runs after a successful check_module. Reports E0901 for constructs that
// stage 0 can check but not compile yet.
void lower_module(Sema *sema, AirProgram *out);

#endif
