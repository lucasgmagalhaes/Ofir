// Type checking of `match` (spec 4.5).
//
// Exhaustiveness is checked conservatively until the usefulness algorithm
// of roadmap 2.16: a match is accepted when an arm without a guard matches
// everything, when `true` and `false` are both covered, or when every
// variant of an enum is covered by an arm whose fields all match anything.
// No non-exhaustive match is accepted; a few exhaustive ones are not yet.

#include "sema/check_internal.h"

static StrView name_of(Checker *c, Symbol name) {
    return symbol_name(&c->sema->session->names, name);
}

// Marks what an unguarded arm's pattern covers.
static void cover(Checker *c, const Pattern *p, bool *variants, bool *bools, bool *all) {
    if (pattern_irrefutable(c, p)) {
        *all = true;
    } else if (p->kind == PAT_OR) {
        for (uint32_t i = 0; i < p->as.alternatives.len; i++) {
            cover(c, p->as.alternatives.items[i], variants, bools, all);
        }
    } else if (p->kind == PAT_VARIANT && variants) {
        bool fields_open = true;
        for (uint32_t i = 0; i < p->as.variant.args.len; i++) {
            fields_open = fields_open && pattern_irrefutable(c, p->as.variant.args.items[i]);
        }
        variants[p->as.variant.index] = variants[p->as.variant.index] || fields_open;
    } else if (p->kind == PAT_LITERAL && p->as.literal->kind == EXPR_BOOL) {
        bools[p->as.literal->as.bool_value] = true;
    }
}

static void report_missing(Checker *c, Expr *e, const EnumInfo *info, const bool *variants) {
    StrBuf missing = {0};
    for (uint32_t i = 0; info && i < info->count; i++) {
        if (!variants[i]) {
            strbuf_appendf(&missing, "%s`." SV_FMT "`", missing.len ? ", " : "",
                           SV_ARG(name_of(c, info->variants[i].name)));
        }
    }
    if (missing.len) {
        check_error(c, "E0500", e->span, "non-exhaustive `match`: missing %s", missing.data);
    } else {
        check_error(c, "E0500", e->span, "non-exhaustive `match`: add a `_` arm");
    }
    strbuf_free(&missing);
}

static void check_exhaustive(Checker *c, Expr *e, TypeId type) {
    DefId id;
    const EnumInfo *info = enum_of(c, type, &id);
    bool *variants = info ? ARENA_ARRAY(&c->sema->session->arena, bool, info->count + 1) : NULL;
    bool bools[2] = {false, false}, all = false;
    for (uint32_t i = 0; i < e->as.match.arms.len; i++) {
        const MatchArm *arm = &e->as.match.arms.items[i];
        if (!arm->guard) {
            cover(c, arm->pattern, variants, bools, &all);
        }
    }
    bool every_variant = info != NULL;
    for (uint32_t i = 0; info && i < info->count; i++) {
        every_variant = every_variant && variants[i];
    }
    bool covered = all || every_variant || (type.id == TID_BOOL && bools[0] && bools[1]) ||
                   type.id == TID_ERROR;
    if (!covered) {
        report_missing(c, e, info, variants);
    }
}

// The type of the arms, like the branches of `if` (spec 4.3).
static TypeId join_arm(Checker *c, Expr *body, TypeId result, TypeId arm) {
    if (result.id == TID_NEVER) {
        return arm;
    }
    if (arm.id != TID_NEVER && !types_compatible(arm, result)) {
        char expected[64];
        snprintf(expected, sizeof expected, "%s", type_name(c, result));
        check_error(c, "E0300", body->span, "mismatched types: expected %s, found %s",
                    expected, type_name(c, arm));
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
        result = join_arm(c, arm->body, result, arm_body(c, arm->body, hint, as_value));
    }
    check_exhaustive(c, e, type);
    return as_value ? result : (result.id == TID_NEVER ? result : type_id(TID_UNIT));
}
