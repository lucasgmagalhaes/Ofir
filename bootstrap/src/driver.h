// The compilation pipeline: runs each phase in order.

#ifndef ASTRA_DRIVER_H
#define ASTRA_DRIVER_H

#include "diag/diag.h"

typedef enum {
    EMIT_NONE,     // full compilation
    EMIT_TOKENS,   // print the tokens and stop
    EMIT_AST,      // print the syntax tree and stop
} Emit;

typedef enum {
    STOP_NEVER,        // run every phase
    STOP_AFTER_LEX,    // --stop-after=lex: for benchmarks, no output
    STOP_AFTER_PARSE,  // --stop-after=parse
} StopAfter;

typedef struct {
    const char *command;   // "build" or "run"
    const char *input;
    const char *output;    // -o
    Emit emit;
    StopAfter stop_after;
    DiagFormat format;
    bool time_passes;      // --time-passes: report the time spent in each phase
} Options;

// Returns the process exit status: 0 on success, 1 on compile errors.
int driver_build(const Options *opts);

#endif
