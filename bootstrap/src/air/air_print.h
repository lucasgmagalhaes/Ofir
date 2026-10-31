// AIR printing and validation.

#ifndef ASTRA_AIR_PRINT_H
#define ASTRA_AIR_PRINT_H

#include "air/air.h"
#include "sema/sema.h"

#include <stdio.h>

void air_print(const AirProgram *program, const Sema *sema, FILE *out);

// Checks the structural rules of AIR: every block is terminated, every
// jump targets an existing block, every local exists. Returns false and
// describes the first problem in `error` (an internal compiler error).
bool air_validate(const AirProgram *program, char *error, size_t size);

#endif
