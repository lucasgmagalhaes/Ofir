// Typed indices for semantic entities (docs/design/compiler-architecture.md,
// 2). The AST stores them as annotations filled in by the resolver and the
// type checker.

#ifndef OFIR_IDS_H
#define OFIR_IDS_H

#include <stdint.h>

typedef struct {
    uint32_t id;    // index in the type table; 0 is the error type
} TypeId;

typedef struct {
    uint32_t id;    // index in the definition table; 0 is "none"
} DefId;

typedef struct {
    uint32_t id;    // index in the locals of one function body
} LocalId;

#endif
