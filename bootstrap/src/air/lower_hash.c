// The intrinsic `hashOf<K: Hash>(k)` of std/core/map.of and `==` on
// bounded parameters: the code is chosen for the instance's concrete type,
// which stage 0 restricts to integers, bool, str and tuples of them (and,
// for `==`, the other types it compares): integers and bools are hashed by
// `hashU64`, strings by `hashStr`, tuples by combining their elements'
// hashes with `hashCombine`, all in std/core/hash.of (deterministic, ADR
// 0011).

#include "air/lower_internal.h"

static const char HASH[] = "std.core.hash";

static TypeId concrete(Lowerer *l, Expr *call) {
    const Ty *targs = type_get(l->types, l->body->targs);
    TypeId type = type_get(l->types, call->as.call.type_args)->args[0];
    return type_subst(l->types, type, targs->args, targs->arg_count);
}

static Operand call_hash(Lowerer *l, const char *name, Operand *args, uint32_t count) {
    TypeId u64 = type_id(TID_U64);
    LocalId dest = new_temp(l, u64);
    Operand *copy = ARENA_ARRAY(&l->sema->session->arena, Operand, count);
    for (uint32_t i = 0; i < count; i++) {
        copy[i] = args[i];
    }
    emit_direct_call(l, std_fn(l, HASH, name), type_id(TID_UNIT), (CallArgs){copy, count},
                     &dest);
    return op_copy(dest, u64);
}

// Element `i` of the tuple in `value`.
static Operand element(Lowerer *l, Operand value, TypeId type, uint32_t i) {
    uint32_t *field = ARENA_NEW(&l->sema->session->arena, uint32_t);
    *field = i;
    Place place = {.local = to_local(l, value), .path = {.fields = field, .len = 1}};
    return place_operand(place, OPERAND_COPY, type_get(l->types, type)->args[i]);
}

static Operand hash_value(Lowerer *l, Operand value, TypeId type) {
    value.type = type;
    const Ty *ty = type_get(l->types, type);
    if (ty->kind == TY_STR) {
        return call_hash(l, "hashStr", &value, 1);
    }
    if (ty->kind == TY_TUPLE) {
        Operand h = op_int(0, type_id(TID_U64));
        for (uint32_t i = 0; i < ty->arg_count; i++) {
            TypeId elem = type_get(l->types, type)->args[i];
            Operand args[2] = {h, hash_value(l, element(l, value, type, i), elem)};
            h = call_hash(l, "hashCombine", args, 2);
        }
        return h;
    }
    TypeId u64 = type_id(TID_U64);   // ints and bool: their bits, without a range check
    Operand bits = op_copy(emit_temp(l, u64, (Rvalue){.kind = RV_CAST, .a = value, .to = u64}),
                           u64);
    return call_hash(l, "hashU64", &bits, 1);
}

Operand lower_equal_values(Lowerer *l, Operand a, Operand b, TypeId type) {
    a.type = b.type = type;
    const Ty *ty = type_get(l->types, type);
    if (ty->kind != TY_TUPLE) {
        return lower_binary_op(l, (Arith){.op = OP_EQ, .type = type, .a = a, .b = b});
    }
    Operand all = op_bool(true);
    for (uint32_t i = 0; i < ty->arg_count; i++) {
        TypeId elem = type_get(l->types, type)->args[i];
        Operand same = lower_equal_values(l, element(l, a, type, i), element(l, b, type, i), elem);
        Rvalue both = {.kind = RV_BINARY, .op = OP_AND, .a = all, .b = same};
        all = op_copy(emit_temp(l, type_id(TID_BOOL), both), type_id(TID_BOOL));
    }
    return all;
}

Operand lower_hash_of(Lowerer *l, Expr *call) {
    return hash_value(l, lower_expr(l, call->as.call.args.items[0].value), concrete(l, call));
}
