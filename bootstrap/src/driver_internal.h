// State shared by the driver files.

#ifndef ASTRA_DRIVER_INTERNAL_H
#define ASTRA_DRIVER_INTERNAL_H

#include "driver.h"
#include "sema/sema.h"
#include "syntax/ast.h"

// Times each phase when `--time-passes` is given.
typedef struct {
    bool enabled;
    double start;   // when the current phase started
} Timer;

void timer_start(Timer *timer);
void timer_report(Timer *timer, const char *phase, const char *detail);

typedef enum {
    CONTINUE,   // go on with the next phase
    STOP,       // the requested output is done, or a phase reported errors
    STOP_FAIL,  // a failure without a diagnostic (clang failed, cannot write a file)
} Next;

#define PATH_SIZE 4096

// One compilation, passed from phase to phase.
typedef struct {
    Session *session;
    const Options *opts;
    Timer *timer;
    FileId file;
    char exe[PATH_SIZE];   // the executable, once built
} Pipeline;

// driver_paths.c: `$env`, else the first of `places` (relative to the
// compiler's directory) that exists.
bool find_installed(const char *argv0, const char *env, const char *const *places,
                    size_t count, char out[PATH_SIZE]);

// driver.c: lexes and parses one file; the root's tokens or AST are what
// --emit prints. NULL when the phases stop here.
Module *parse_file(Pipeline *p, FileId file, bool root, Next *next);

// driver_modules.c: the root module and every module it imports.
Program load_program(Pipeline *p, Module *root);

// driver_backend.c: lowering, code generation and clang.
Next run_backend(Pipeline *p, Sema *sema);

// Runs the program and returns its exit status (128 + signal if killed).
int run_program(const char *exe);

#endif
