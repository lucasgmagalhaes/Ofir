// State shared by every phase of one compilation.

#ifndef OFIR_SESSION_H
#define OFIR_SESSION_H

#include "base/arena.h"
#include "base/intern.h"
#include "base/source.h"
#include "diag/diag.h"

typedef struct {
    Arena arena;           // almost all compiler data lives here
    SourceMap sources;
    Interner names;
    Diagnostics diags;
} Session;

void session_init(Session *session);
void session_free(Session *session);

#endif
