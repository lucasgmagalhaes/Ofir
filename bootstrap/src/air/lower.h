// Lowering: the typed AST of a module → AIR (one body per function).

#ifndef OFIR_LOWER_H
#define OFIR_LOWER_H

#include "air/air.h"
#include "sema/sema.h"

// Receives each function as soon as it is lowered; the body is freed after
// the call, so only one function is in memory at a time. The sink may change it.
typedef void (*BodySink)(void *context, AirBody *body);

// Runs after a successful check_program. Reports E0901 for constructs that
// stage 0 can check but not compile yet.
void lower_program(Sema *sema, BodySink sink, void *context);

#endif
