// Clones (spec 5.1, roadmap 3.3): clone glue and the derived `.clone()`.
//
// A value of a type that needs no drop is cloned by copying its bits. The
// others are cloned by their *clone glue*, a function `clone<T>(value: T) T`
// lowered once per concrete type like the drop glue: it calls T's `clone`
// method when T declares one (str, List and Box in std, and user types),
// and otherwise clones the fields one by one (of the active variant, for an
// enum). The checker ensures every part can be cloned (sema/check_clone.c).

#include "air/lower_internal.h"

static Operand *one_operand(Lowerer *l, Operand value) {
    Operand *arg = ARENA_NEW(&l->sema->session->arena, Operand);
    *arg = value;
    return arg;
}

// A clone of `value`, of the concrete type `type`; `value` is only read.
Operand clone_value(Lowerer *l, Operand value, TypeId type) {
    value.type = type;
    if (!needs_drop(l, type)) {
        return value;   // nothing owned: the bits are the clone
    }
    LocalId dest = new_temp(l, type);
    TypeId targs = type_tuple(l->types, &type, 1);
    emit_direct_call(l, &(FnDecl){.def = {GLUE_CLONE}}, targs,
                     (CallArgs){one_operand(l, value), 1}, &dest);
    return op_copy(dest, type);
}

static Operand clone_part(Lowerer *l, Place place, TypeId type) {
    return clone_value(l, place_operand(place, OPERAND_COPY, type), type);
}

// The struct or tuple in local 1, field by field, into local 0.
static void clone_fields(Lowerer *l, TypeId type) {
    const Ty *ty = type_get(l->types, type);
    uint32_t count = ty->kind == TY_TUPLE
                         ? ty->arg_count
                         : sema_def(l->sema, (DefId){ty->data})->fields->count;
    Operand *items = ARENA_ARRAY(&l->sema->session->arena, Operand, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        TypeId field = type_get(l->types, type)->kind == TY_TUPLE
                           ? type_get(l->types, type)->args[i]
                           : struct_field_type(l->sema, type, i);
        items[i] = clone_part(l, place_child(l, (Place){.local = {1}}, i), field);
    }
    emit_assign(l, (LocalId){0}, (Rvalue){.kind = RV_AGGREGATE, .items = items, .count = count});
}

// The enum in local 1: a test per variant, which rebuilds it with clones.
static void clone_variants(Lowerer *l, TypeId type) {
    const Def *def = sema_def(l->sema, (DefId){type_get(l->types, type)->data});
    Place value = {.local = {1}};
    BlockId join = air_new_block(l->body);
    for (uint32_t v = 0; v < def->variants->count; v++) {
        BlockId build = air_new_block(l->body), next = air_new_block(l->body);
        terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = is_variant(l, value, type, v),
                                  .target = build, .other = next});
        switch_to(l, build);
        uint32_t count = def->variants->variants[v].count;
        Operand *fields = ARENA_ARRAY(&l->sema->session->arena, Operand, count + 1);
        for (uint32_t i = 0; i < count; i++) {
            fields[i] = clone_part(l, variant_field(l, value, v, i),
                                   variant_field_type(l->sema, type, v, i));
        }
        emit_assign(l, (LocalId){0},
                    (Rvalue){.kind = RV_USE, .a = variant_value(l, type, v, fields, count)});
        emit_goto(l, join);
        switch_to(l, next);
    }
    terminate(l, (Terminator){.kind = TERM_UNREACHABLE});
    switch_to(l, join);
}

static void clone_body(Lowerer *l, TypeId type) {
    FnDecl *method = type_method(l, type, SV("clone"));
    const Ty *ty = type_get(l->types, type);
    if (method) {
        LocalId result = {0};
        emit_direct_call(l, method, method_targs(l, type),
                         (CallArgs){one_operand(l, op_copy((LocalId){1}, type)), 1}, &result);
    } else if (ty->kind == TY_TUPLE ||
               (ty->kind == TY_ADT && sema_def(l->sema, (DefId){ty->data})->kind == DEF_STRUCT)) {
        clone_fields(l, type);
    } else {
        clone_variants(l, type);
    }
}

void lower_clone_glue(Lowerer *l, TypeId targs, BodySink sink, void *context) {
    TypeId type = type_get(l->types, targs)->args[0];
    AirBody body = {.def = {GLUE_CLONE}, .targs = targs, .param_count = 1};
    l->body = &body;
    air_new_local(&body, type, (Symbol){0}, (Span){0});
    air_new_local(&body, type, (Symbol){0}, (Span){0});
    l->block = air_new_block(&body);
    clone_body(l, type);
    air_terminate(&body, l->block, (Terminator){.kind = TERM_RETURN});
    mono_body(l, &body);
    sink(context, &body);
    air_body_free(&body);
}

// `x.clone()` on a type without a `clone` method.
Operand lower_clone_call(Lowerer *l, Expr *call) {
    Operand self = lower_expr(l, call->as.call.callee->as.field.base);
    return clone_value(l, self, concrete_type(l, call->type));
}

// `cloneOf<T>(value)` (std/core/clone.of): T is known in the instance.
Operand lower_clone_of(Lowerer *l, Expr *call) {
    Operand value = lower_expr(l, call->as.call.args.items[0].value);
    TypeId type = concrete_type(l, type_get(l->types, call->as.call.type_args)->args[0]);
    return clone_value(l, value, type);
}
