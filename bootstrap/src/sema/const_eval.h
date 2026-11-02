// Compile-time evaluation of constants (spec 3.2): literals, other
// constants, the constants of the primitive types and the arithmetic,
// bitwise, comparison and logical operators, with the same overflow,
// division and shift rules as at run time, reported as errors instead of
// panics.

#ifndef OFIR_CONST_EVAL_H
#define OFIR_CONST_EVAL_H

#include "base/str.h"
#include "sema/ids.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum { CV_INT, CV_FLOAT, CV_BOOL, CV_STR } ConstKind;

typedef struct ConstValue {
    ConstKind kind;
    TypeId type;
    uint64_t bits;   // integers: the two's complement bit pattern of the type's width
    double f;
    bool b;
    StrView s;       // in the session arena
} ConstValue;

#endif
