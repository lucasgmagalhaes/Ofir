// State shared by the driver files.

#ifndef ASTRA_DRIVER_INTERNAL_H
#define ASTRA_DRIVER_INTERNAL_H

#include "driver.h"
#include "sema/sema.h"

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

// driver_backend.c: lowering, code generation and clang.
Next run_backend(Pipeline *p, Sema *sema);

// Runs the program and returns its exit status (128 + signal if killed).
int run_program(const char *exe);

#endif
