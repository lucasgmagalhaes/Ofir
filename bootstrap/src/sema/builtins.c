#include "sema/builtins.h"

#include <string.h>

static const BuiltinInfo TABLE[] = {
    {"new", RECV_STR_TYPE, BM_STR_NEW, MODE_BORROW, {BT_NONE, BT_NONE}, BT_STR, false},
    {"len", RECV_STR, BM_STR_LEN, MODE_BORROW, {BT_NONE, BT_NONE}, BT_INT, false},
    {"isEmpty", RECV_STR, BM_STR_IS_EMPTY, MODE_BORROW, {BT_NONE, BT_NONE}, BT_BOOL, false},
    {"push", RECV_STR, BM_STR_PUSH, MODE_MUT, {BT_STR, BT_NONE}, BT_NONE, false},
    {"clear", RECV_STR, BM_STR_CLEAR, MODE_MUT, {BT_NONE, BT_NONE}, BT_NONE, false},
    {"startsWith", RECV_STR, BM_STR_STARTS_WITH, MODE_BORROW, {BT_STR, BT_NONE}, BT_BOOL, false},
    {"endsWith", RECV_STR, BM_STR_ENDS_WITH, MODE_BORROW, {BT_STR, BT_NONE}, BT_BOOL, false},
    {"contains", RECV_STR, BM_STR_CONTAINS, MODE_BORROW, {BT_STR, BT_NONE}, BT_BOOL, false},
    {"repeat", RECV_STR, BM_STR_REPEAT, MODE_BORROW, {BT_INT, BT_NONE}, BT_STR, false},
    {"toLower", RECV_STR, BM_STR_TO_LOWER, MODE_BORROW, {BT_NONE, BT_NONE}, BT_STR, false},
    {"toUpper", RECV_STR, BM_STR_TO_UPPER, MODE_BORROW, {BT_NONE, BT_NONE}, BT_STR, false},
    {"toOwned", RECV_STR, BM_STR_TO_OWNED, MODE_BORROW, {BT_NONE, BT_NONE}, BT_STR, false},
    {"wrappingAdd", RECV_INT, BM_WRAPPING_ADD, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"wrappingSub", RECV_INT, BM_WRAPPING_SUB, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"wrappingMul", RECV_INT, BM_WRAPPING_MUL, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"abs", RECV_INT, BM_ABS, MODE_BORROW, {BT_NONE, BT_NONE}, BT_SELF, false},
    {"min", RECV_INT, BM_MIN, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"max", RECV_INT, BM_MAX, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"clamp", RECV_INT, BM_CLAMP, MODE_BORROW, {BT_SELF, BT_SELF}, BT_SELF, false},
    {"pow", RECV_INT, BM_POW, MODE_BORROW, {BT_U32, BT_NONE}, BT_SELF, false},
    {"wrappingCast", RECV_INT, BM_WRAPPING_CAST, MODE_BORROW, {BT_NONE, BT_NONE}, BT_TARGET, true},
    {"saturatingCast", RECV_INT, BM_SATURATING_CAST, MODE_BORROW, {BT_NONE, BT_NONE}, BT_TARGET,
     true},
    {"sqrt", RECV_FLOAT, BM_SQRT, MODE_BORROW, {BT_NONE, BT_NONE}, BT_SELF, false},
    {"abs", RECV_FLOAT, BM_FABS, MODE_BORROW, {BT_NONE, BT_NONE}, BT_SELF, false},
    {"floor", RECV_FLOAT, BM_FLOOR, MODE_BORROW, {BT_NONE, BT_NONE}, BT_SELF, false},
    {"ceil", RECV_FLOAT, BM_CEIL, MODE_BORROW, {BT_NONE, BT_NONE}, BT_SELF, false},
    {"min", RECV_FLOAT, BM_FMIN, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"max", RECV_FLOAT, BM_FMAX, MODE_BORROW, {BT_SELF, BT_NONE}, BT_SELF, false},
    {"isNan", RECV_FLOAT, BM_IS_NAN, MODE_BORROW, {BT_NONE, BT_NONE}, BT_BOOL, false},
    {"add", RECV_PTR, BM_PTR_ADD, MODE_BORROW, {BT_INT, BT_NONE}, BT_SELF, false},
};

const BuiltinInfo *builtin_method(const Interner *names, Receiver receiver, Symbol name) {
    StrView text = symbol_name(names, name);
    for (size_t i = 0; i < ARRAY_LEN(TABLE); i++) {
        if (TABLE[i].receiver == receiver && sv_eq(text, sv_from_cstr(TABLE[i].name))) {
            return &TABLE[i];
        }
    }
    return NULL;
}

const BuiltinInfo *builtin_info(BuiltinMethod method) {
    for (size_t i = 0; i < ARRAY_LEN(TABLE); i++) {
        if (TABLE[i].method == method) {
            return &TABLE[i];
        }
    }
    return NULL;
}

BuiltinConst builtin_const(const Interner *names, const Ty *type, Symbol name) {
    StrView text = symbol_name(names, name);
    if (type->kind == TY_INT) {
        return sv_eq(text, SV("MIN")) ? BC_MIN : sv_eq(text, SV("MAX")) ? BC_MAX : BC_NONE;
    }
    if (type->kind == TY_FLOAT) {
        return sv_eq(text, SV("NAN"))        ? BC_NAN
               : sv_eq(text, SV("INFINITY")) ? BC_INFINITY
               : sv_eq(text, SV("EPSILON"))  ? BC_EPSILON
                                             : BC_NONE;
    }
    return BC_NONE;
}
