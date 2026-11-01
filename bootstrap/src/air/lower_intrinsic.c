// Lowering of the intrinsics that std declares in `extern "intrinsic"`
// blocks (ADR 0019): `wrapping_add_i64(a, b)`, `sqrt_f64(x)`, ... The name
// gives the operation; the signature gives the types.

#include "air/lower_internal.h"

#include <string.h>

typedef struct {
    const char *prefix;   // the name without its type suffix
    RvalueKind kind;
    Op op;
    FloatFn fn;
} Intrinsic;

static const Intrinsic INTRINSICS[] = {
    {"wrapping_add_", RV_BINARY, OP_ADD, 0},
    {"wrapping_sub_", RV_BINARY, OP_SUB, 0},
    {"wrapping_mul_", RV_BINARY, OP_MUL, 0},
    {"sqrt_", RV_FLOAT_FN, 0, FF_SQRT},
    {"fabs_", RV_FLOAT_FN, 0, FF_FABS},
    {"floor_", RV_FLOAT_FN, 0, FF_FLOOR},
    {"ceil_", RV_FLOAT_FN, 0, FF_CEIL},
    {"minnum_", RV_FLOAT_FN, 0, FF_MINNUM},
    {"maxnum_", RV_FLOAT_FN, 0, FF_MAXNUM},
};

static const Intrinsic *find(StrView name) {
    for (size_t i = 0; i < ARRAY_LEN(INTRINSICS); i++) {
        size_t len = strlen(INTRINSICS[i].prefix);
        if (name.len > len && memcmp(name.ptr, INTRINSICS[i].prefix, len) == 0) {
            return &INTRINSICS[i];
        }
    }
    return NULL;
}

// `sizeOf<T>()`: its type argument is substituted with the instance's.
static Operand size_of(Lowerer *l, Expr *call) {
    TypeId type = type_get(l->types, call->as.call.type_args)->args[0];
    Rvalue rv = {.kind = RV_SIZE_OF, .to = type};
    return op_copy(emit_temp(l, type_id(TID_INT), rv), type_id(TID_INT));
}

Operand lower_intrinsic(Lowerer *l, Expr *call, FnDecl *fn) {
    StrView name = symbol_name(&l->sema->session->names, fn->name);
    if (sv_eq(name, SV("sizeOf"))) {
        return size_of(l, call);
    }
    const Intrinsic *op = find(name);
    if (!op) {
        diag_error(&l->sema->session->diags, "E0805", fn->span, "unknown intrinsic");
        return op_unit();
    }
    ArgList args = call->as.call.args;
    Operand a = lower_expr(l, args.items[0].value);
    Operand b = args.len > 1 ? lower_expr(l, args.items[1].value) : op_unit();
    Rvalue rv = {.kind = op->kind, .op = op->op, .fn = op->fn, .a = a, .b = b};
    return op_copy(emit_temp(l, call->type, rv), call->type);
}
