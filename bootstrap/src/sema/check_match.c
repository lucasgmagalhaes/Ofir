// Type checking of `match` (spec 4.5); exhaustiveness is in exhaustive.c.

#include "sema/check_internal.h"

// The type of the arms, like the branches of `if` (spec 4.3).
static TypeId join_arm(Checker *c, Expr *body, TypeId expected, TypeId result, TypeId arm) {
    if (result.id == TID_NEVER) {
        return arm;
    }
    if (arm.id != TID_NEVER && !types_compatible(arm, result)) {
        char first[64];
        snprintf(first, sizeof first, "%s", type_name(c, result));
        mismatched_branches(c, body->span, expected, first, type_name(c, arm));
    }
    return result;
}

static TypeId arm_body(Checker *c, Expr *body, TypeId expected, bool as_value) {
    if (!as_value && body->kind == EXPR_UNSAFE) {
        body->type = check_unsafe_block(c, body, NO_TYPE, false);
        return body->type.id == TID_NEVER ? body->type : type_id(TID_UNIT);
    }
    if (!as_value && body->kind == EXPR_BLOCK) {
        body->type = check_block(c, body->as.block, NO_TYPE, false);
        return body->type.id == TID_NEVER ? body->type : type_id(TID_UNIT);
    }
    if (!as_value && body->kind == EXPR_IF) {
        body->type = check_if(c, body, NO_TYPE, false);
        return body->type;
    }
    TypeId type = check_expr(c, body, expected);
    return as_value || type.id == TID_NEVER ? type : type_id(TID_UNIT);
}

TypeId check_match(Checker *c, Expr *e, TypeId expected, bool as_value) {
    TypeId type = check_expr(c, e->as.match.scrutinee, NO_TYPE);
    TypeId result = type_id(TID_NEVER);
    for (uint32_t i = 0; i < e->as.match.arms.len; i++) {
        MatchArm *arm = &e->as.match.arms.items[i];
        check_pattern(c, arm->pattern, type);
        if (arm->guard) {
            coerce(c, arm->guard, type_id(TID_BOOL));
        }
        TypeId hint = result.id == TID_NEVER ? expected : result;
        result = join_arm(c, arm->body, expected, result, arm_body(c, arm->body, hint, as_value));
    }
    check_exhaustive(c, e, type);
    return as_value ? result : (result.id == TID_NEVER ? result : type_id(TID_UNIT));
}
