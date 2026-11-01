// The compilation pipeline: runs each phase in order.

#ifndef ASTRA_DRIVER_H
#define ASTRA_DRIVER_H

#include "diag/diag.h"

typedef enum {
    EMIT_NONE,     // full compilation
    EMIT_TOKENS,   // print the tokens and stop
    EMIT_AST,      // print the syntax tree and stop
    EMIT_NAMES,    // print what the names of the root module resolve to, and stop
    EMIT_TYPES,    // print the types of the root module's declarations and locals, and stop
    EMIT_AIR,      // print the AIR and stop
    EMIT_LLVM,     // write <output>.ll and stop
} Emit;

typedef enum {
    STOP_NEVER,        // run every phase
    STOP_AFTER_LEX,    // --stop-after=lex: for benchmarks, no output
    STOP_AFTER_PARSE,  // --stop-after=parse
    STOP_AFTER_CHECK,  // --stop-after=check: name resolution and type checking
} StopAfter;

typedef struct {
    const char *command;   // "build", "run" or "test"
    const char *input;
    const char *output;    // -o
    Emit emit;
    StopAfter stop_after;
    DiagFormat format;
    bool time_passes;      // --time-passes: report the time spent in each phase
    bool release;          // --release: optimize (clang -O2)
    bool deny_warnings;    // --deny-warnings: warnings are errors (spec 12.6)
    const char *argv0;     // to find the runtime next to the compiler
} Options;

// Returns the process exit status: 0 on success, 1 on compile errors.
int driver_build(const Options *opts);

#endif
