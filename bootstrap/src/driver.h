// The compilation pipeline: runs each phase in order.

#ifndef ASTRA_DRIVER_H
#define ASTRA_DRIVER_H

#include "diag/diag.h"

typedef enum {
    EMIT_NONE,     // full compilation
    EMIT_TOKENS,   // print the tokens and stop
} Emit;

typedef struct {
    const char *command;   // "build" or "run"
    const char *input;
    const char *output;    // -o
    Emit emit;
    DiagFormat format;
} Options;

// Returns the process exit status: 0 on success, 1 on compile errors.
int driver_build(const Options *opts);

#endif
