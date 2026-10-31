// Built-in methods and constants of the primitive types (docs/std/core.md
// and strings.md, the entries marked S0): `s.len()`, `x.wrappingAdd(y)`,
// `x.wrappingCast<u8>()`, `str.new()`, `i32.MAX`, ...

#ifndef ASTRA_BUILTINS_H
#define ASTRA_BUILTINS_H

#include "base/intern.h"
#include "sema/types.h"
#include "syntax/ast.h"

typedef enum {
    // str
    BM_STR_NEW, BM_STR_LEN, BM_STR_IS_EMPTY, BM_STR_PUSH, BM_STR_CLEAR, BM_STR_STARTS_WITH,
    BM_STR_ENDS_WITH, BM_STR_CONTAINS, BM_STR_REPEAT, BM_STR_TO_LOWER, BM_STR_TO_UPPER,
    BM_STR_TO_OWNED,
    // integers
    BM_WRAPPING_ADD, BM_WRAPPING_SUB, BM_WRAPPING_MUL, BM_ABS, BM_MIN, BM_MAX, BM_CLAMP, BM_POW,
    BM_WRAPPING_CAST, BM_SATURATING_CAST,
    // floats
    BM_SQRT, BM_FABS, BM_FLOOR, BM_CEIL, BM_FMIN, BM_FMAX, BM_IS_NAN,
    BM_COUNT,
} BuiltinMethod;

// How a parameter or the result relates to the receiver type.
typedef enum {
    BT_NONE,      // no value (no parameter / returns ())
    BT_SELF,      // the receiver type
    BT_STR, BT_INT, BT_BOOL, BT_U32,
    BT_TARGET,    // the type argument: x.wrappingCast<T>() returns T
} BuiltinType;

typedef enum { RECV_STR, RECV_INT, RECV_FLOAT, RECV_STR_TYPE } Receiver;

typedef struct {
    const char *name;
    Receiver receiver;     // RECV_STR_TYPE: an associated function, `str.new()`
    BuiltinMethod method;
    Mode self_mode;        // MODE_MUT: the receiver must be a mutable place
    BuiltinType params[2];
    BuiltinType ret;
    bool type_arg;         // takes one integer type argument
} BuiltinInfo;

// The method `name` for a receiver, or NULL.
const BuiltinInfo *builtin_method(const Interner *names, Receiver receiver, Symbol name);
const BuiltinInfo *builtin_info(BuiltinMethod method);

typedef enum { BC_NONE, BC_MIN, BC_MAX, BC_NAN, BC_INFINITY, BC_EPSILON } BuiltinConst;

// `I.MIN`, `I.MAX`, `F.NAN`, `F.INFINITY`, `F.EPSILON`.
BuiltinConst builtin_const(const Interner *names, const Ty *type, Symbol name);

#endif
