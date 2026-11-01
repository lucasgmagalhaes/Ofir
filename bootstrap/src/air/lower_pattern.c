// Lowering of tuples, enum variants, patterns and `match` (spec 4.5).
//
// A pattern is a sequence of tests on the value's place: each failed test
// jumps to the next arm; bindings are assigned along the way. Arms are
// tried in order (LLVM turns chains of tag comparisons into a switch).

#include "air/lower_internal.h"

static Place child(Lowerer *l, Place place, uint32_t step) {
    uint32_t *fields = ARENA_ARRAY(&l->sema->session->arena, uint32_t, place.path.len + 2);
    for (uint32_t i = 0; i < place.path.len; i++) {
        fields[i] = place.path.fields[i];
    }
    fields[place.path.len] = step;
    place.path.fields = fields;
    place.path.len++;
    return place;
}

static const EnumInfo *enum_info(Lowerer *l, TypeId type) {
    return sema_def(l->sema, (DefId){type_get(l->types, type)->data})->variants;
}

static Operand tag_of(Lowerer *l, Place place) {
    return place_operand(child(l, place, 0), OPERAND_COPY, type_id(TID_I32));
}

static Operand discriminant(Lowerer *l, TypeId type, uint32_t index) {
    return op_int((uint64_t)enum_info(l, type)->variants[index].discriminant, type_id(TID_I32));
}

// A variant value: the tag, then each payload field.
Operand variant_value(Lowerer *l, TypeId type, uint32_t index, const Operand *fields,
                      uint32_t count) {
    LocalId dest = new_temp(l, type);
    Place place = {.local = dest};
    for (uint32_t i = 0; i < count; i++) {
        emit_store(l, child(l, child(l, place, PATH_VARIANT | index), i), fields[i]);
    }
    emit_store(l, child(l, place, 0), discriminant(l, type, index));
    return op_copy(dest, type);
}

Operand build_variant(Lowerer *l, TypeId type, uint32_t index, ArgList args) {
    uint32_t count = enum_info(l, type)->variants[index].count;
    count = args.len < count ? args.len : count;
    Operand *fields = ARENA_ARRAY(&l->sema->session->arena, Operand, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        fields[i] = lower_expr(l, args.items[i].value);
    }
    return variant_value(l, type, index, fields, count);
}

// Field `field` of variant `index` of the enum value at `place`.
Place variant_field(Lowerer *l, Place place, uint32_t index, uint32_t field) {
    return child(l, child(l, place, PATH_VARIANT | index), field);
}

// `a == b` on enums without payloads compares the tags.
Operand enum_equality(Lowerer *l, Op op, Operand a, Operand b) {
    Place pa = {.local = to_local(l, a)}, pb = {.local = to_local(l, b)};
    Rvalue rv = {.kind = RV_BINARY, .op = op, .a = tag_of(l, pa), .b = tag_of(l, pb)};
    return op_copy(emit_temp(l, type_id(TID_BOOL), rv), type_id(TID_BOOL));
}

Operand lower_tuple(Lowerer *l, Expr *e) {
    uint32_t count = e->as.elems.len;
    if (count == 0) {
        return op_unit();
    }
    Operand *items = ARENA_ARRAY(&l->sema->session->arena, Operand, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        items[i] = lower_expr(l, e->as.elems.items[i]);
        if (i + 1 < count) {
            items[i] = settle(l, items[i], e->as.elems.items[i + 1]);
        }
    }
    Rvalue rv = {.kind = RV_AGGREGATE, .items = items, .count = count};
    return op_copy(emit_temp(l, e->type, rv), e->type);
}

static void test(Lowerer *l, Operand cond, BlockId fail) {
    BlockId next = air_new_block(l->body);
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = cond, .target = next, .other = fail});
    switch_to(l, next);
}

static Operand compare(Lowerer *l, Op op, TypeId type, Operand a, Operand b) {
    return lower_binary_op(l, (Arith){.op = op, .type = type, .a = a, .b = b});
}

// Whether the enum value at `place`, of type `type`, is variant `index`.
Operand is_variant(Lowerer *l, Place place, TypeId type, uint32_t index) {
    return compare(l, OP_EQ, type_id(TID_I32), tag_of(l, place), discriminant(l, type, index));
}

static void lower_patterns(Lowerer *l, PatternList list, Place place, uint32_t prefix,
                           BlockId fail);

static void or_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail) {
    BlockId ok = air_new_block(l->body);
    uint32_t count = p->as.alternatives.len;
    for (uint32_t i = 0; i < count; i++) {
        BlockId next = i + 1 < count ? air_new_block(l->body) : fail;
        lower_pattern(l, p->as.alternatives.items[i], place, next);
        emit_goto(l, ok);
        if (i + 1 < count) {
            switch_to(l, next);
        }
    }
    switch_to(l, ok);
}

static void struct_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail) {
    const Ty *ty = type_get(l->types, p->type);
    const StructInfo *info = sema_def(l->sema, (DefId){ty->data})->fields;
    for (uint32_t i = 0; i < p->as.struct_.fields.len; i++) {
        FieldPattern *f = &p->as.struct_.fields.items[i];
        uint32_t index = 0;
        while (index + 1 < info->count && !symbol_eq(info->names[index], f->name)) {
            index++;
        }
        lower_pattern(l, f->pattern, child(l, place, index), fail);
    }
}

static void variant_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail) {
    uint32_t index = p->as.variant.index;
    Operand is = compare(l, OP_EQ, type_id(TID_I32), tag_of(l, place),
                         discriminant(l, p->type, index));
    test(l, is, fail);
    lower_patterns(l, p->as.variant.args, place, PATH_VARIANT | index, fail);
}

static void range_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail) {
    Operand value = place_operand(place, OPERAND_COPY, p->type);
    test(l, compare(l, OP_GE, p->type, value, lower_expr(l, p->as.range.lo)), fail);
    Op high = p->as.range.inclusive ? OP_LE : OP_LT;
    test(l, compare(l, high, p->type, value, lower_expr(l, p->as.range.hi)), fail);
}

void lower_pattern(Lowerer *l, Pattern *p, Place place, BlockId fail) {
    switch (p->kind) {
    case PAT_WILDCARD: break;
    case PAT_BINDING:
        emit_store(l, (Place){.local = air_local_of(p->as.binding.local)},
                   place_operand(place, OPERAND_COPY, p->type));
        break;
    case PAT_LITERAL: {
        Operand value = place_operand(place, OPERAND_COPY, p->type);
        test(l, compare(l, OP_EQ, p->type, value, lower_expr(l, p->as.literal)), fail);
        break;
    }
    case PAT_RANGE: range_pattern(l, p, place, fail); break;
    case PAT_TUPLE: lower_patterns(l, p->as.tuple, place, 0, fail); break;
    case PAT_TUPLE_STRUCT: lower_patterns(l, p->as.tuple_struct.args, place, 0, fail); break;
    case PAT_STRUCT: struct_pattern(l, p, place, fail); break;
    case PAT_VARIANT: variant_pattern(l, p, place, fail); break;
    case PAT_OR: or_pattern(l, p, place, fail); break;
    }
}

// Sub-patterns of positional fields; `prefix` views a variant's payload.
static void lower_patterns(Lowerer *l, PatternList list, Place place, uint32_t prefix,
                           BlockId fail) {
    Place base = prefix ? child(l, place, prefix) : place;
    for (uint32_t i = 0; i < list.len; i++) {
        lower_pattern(l, list.items[i], child(l, base, i), fail);
    }
}

static Operand arm_value(Lowerer *l, Expr *body, bool as_value) {
    if (body->kind == EXPR_BLOCK || body->kind == EXPR_UNSAFE) {
        return lower_block(l, body->as.block, as_value);
    }
    return lower_expr(l, body);
}

Operand lower_match(Lowerer *l, Expr *e) {
    bool as_value = type_is_value(e->type);
    LocalId dest = as_value ? new_temp(l, e->type) : (LocalId){0};
    Place place = {.local = to_local(l, lower_expr(l, e->as.match.scrutinee))};
    BlockId join = air_new_block(l->body);
    for (uint32_t i = 0; i < e->as.match.arms.len; i++) {
        MatchArm *arm = &e->as.match.arms.items[i];
        BlockId next = air_new_block(l->body);
        lower_pattern(l, arm->pattern, place, next);
        if (arm->guard) {
            test(l, lower_expr(l, arm->guard), next);
        }
        Operand value = arm_value(l, arm->body, as_value);
        if (as_value && value.kind != OPERAND_UNIT) {
            emit_store(l, (Place){.local = dest}, value);
        }
        emit_goto(l, join);
        switch_to(l, next);
    }
    terminate(l, (Terminator){.kind = TERM_UNREACHABLE});   // the match is exhaustive
    switch_to(l, join);
    return as_value ? op_copy(dest, e->type) : op_unit();
}
