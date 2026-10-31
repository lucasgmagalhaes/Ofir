// Common definitions shared by every part of the stage 0 compiler.

#ifndef ASTRA_COMMON_H
#define ASTRA_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// Allocation that never returns NULL: the compiler exits on out-of-memory.
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);

// Reports an internal compiler error and aborts.
_Noreturn void fatal(const char *format, ...);

#endif
