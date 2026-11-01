// Lowering of statements and blocks.

#include "air/lower_internal.h"

static void assign_operand(Lowerer *l, LocalId dest, Operand value) {
    if (value.kind != OPERAND_UNIT) {
        emit_assign(l, dest, (Rvalue){.kind = RV_USE, .a = value});
    }
}

static void lower_let(Lowerer *l, Stmt *s) {
    Operand value = lower_expr(l, s->as.let.value);
    Pattern *pattern = s->as.let.pattern;
    if (pattern->kind == PAT_BINDING) {
        assign_operand(l, air_local_of(pattern->as.binding.local), value);
    } else if (pattern->kind != PAT_WILDCARD && value.kind != OPERAND_UNIT) {
        // On a mismatch: the `else` block, which cannot fall through; without
        // `else` the pattern always matches (checked).
        BlockId fail = air_new_block(l->body);
        lower_pattern(l, pattern, (Place){.local = to_local(l, value)}, fail);
        BlockId next = air_new_block(l->body);
        emit_goto(l, next);
        switch_to(l, fail);
        if (s->as.let.else_) {
            lower_block(l, s->as.let.else_, false);
        }
        terminate(l, (Terminator){.kind = TERM_UNREACHABLE});
        switch_to(l, next);
    }
}

static void lower_assign(Lowerer *l, Stmt *s) {
    Expr *target = s->as.assign.target;
    Place place = lower_place(l, target);
    Operand value = lower_expr(l, s->as.assign.value);
    if (s->as.assign.op == OP_ADD && target->type.id == TID_STR) {   // s += t appends in place
        Operand part = {.kind = OPERAND_ADDR, .type = value.type, .as.local = to_local(l, value)};
        Operand args[] = {place_operand(place, OPERAND_ADDR, target->type), part};
        emit_runtime(l, RT_STR_PUSH_STR, args, 2, NULL);
        return;
    }
    if (s->as.assign.op != OP_ASSIGN) {   // x op= y is x = x op y
        Operand current = place_operand(place, OPERAND_COPY, target->type);
        Arith x = {.op = s->as.assign.op, .type = target->type, .a = current, .b = value,
                   .span = s->span};
        value = lower_binary_op(l, x);
    }
    if (value.kind != OPERAND_UNIT) {
        emit_store(l, place, value);
    }
}

// The body of a loop; `latch` is where `continue` goes.
void lower_loop_body(Lowerer *l, Block *body, BlockId latch, BlockId exit) {
    VEC_PUSH(l->loops, ((Loop){.break_to = exit, .continue_to = latch}));
    lower_block(l, body, false);
    l->loops.len--;
    emit_goto(l, latch);
}

// `for { }` and `for cond { }`.
static void lower_while(Lowerer *l, Stmt *s) {
    BlockId header = air_new_block(l->body), body = air_new_block(l->body);
    BlockId exit = air_new_block(l->body);
    emit_goto(l, header);
    switch_to(l, header);
    if (s->as.for_.kind == FOR_COND) {
        Operand cond = lower_expr(l, s->as.for_.expr);
        terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = cond, .target = body,
                                  .other = exit});
    } else {
        emit_goto(l, body);
    }
    switch_to(l, body);
    lower_loop_body(l, s->as.for_.body, header, exit);
    switch_to(l, exit);
}

// `for i in lo..hi`: a hidden counter, copied into `i` at each iteration so
// that `for mut i` cannot change the iteration. The increment cannot
// overflow: it happens only while the counter is below `hi`.
static void lower_for_range(Lowerer *l, Stmt *s) {
    Expr *range = s->as.for_.expr;
    TypeId type = range->type;
    // Both bounds are evaluated once, before the loop.
    LocalId counter = emit_temp(l, type, (Rvalue){.kind = RV_USE,
                                                  .a = lower_expr(l, range->as.range.lo)});
    LocalId end = emit_temp(l, type, (Rvalue){.kind = RV_USE,
                                              .a = lower_expr(l, range->as.range.hi)});
    Operand hi = op_copy(end, type);
    Operand current = op_copy(counter, type);
    BlockId header = air_new_block(l->body), body = air_new_block(l->body);
    BlockId latch = air_new_block(l->body), exit = air_new_block(l->body);
    bool inclusive = range->as.range.inclusive;
    emit_goto(l, header);
    switch_to(l, header);   // inclusive: enter if lo <= hi; exclusive: loop while i < hi
    Rvalue test = {.kind = RV_BINARY, .op = inclusive ? OP_LE : OP_LT, .a = current, .b = hi};
    Operand go = op_copy(emit_temp(l, type_id(TID_BOOL), test), type_id(TID_BOOL));
    terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = go, .target = body, .other = exit});
    switch_to(l, body);
    emit_assign(l, air_local_of(s->as.for_.first_local), (Rvalue){.kind = RV_USE, .a = current});
    lower_loop_body(l, s->as.for_.body, latch, exit);
    switch_to(l, latch);
    BlockId step = air_new_block(l->body);
    if (inclusive) {   // stop at hi itself, before an increment that could overflow
        Rvalue last = {.kind = RV_BINARY, .op = OP_EQ, .a = current, .b = hi};
        Operand done = op_copy(emit_temp(l, type_id(TID_BOOL), last), type_id(TID_BOOL));
        terminate(l, (Terminator){.kind = TERM_BRANCH, .cond = done, .target = exit,
                                  .other = step});
    } else {
        emit_goto(l, step);
    }
    switch_to(l, step);
    emit_assign(l, counter, (Rvalue){.kind = RV_BINARY, .op = OP_ADD, .a = current,
                                     .b = op_int(1, type)});
    emit_goto(l, inclusive ? body : header);
    switch_to(l, exit);
}

static void lower_jump(Lowerer *l, Stmt *s) {
    Loop loop = VEC_LAST(l->loops);
    emit_goto(l, s->kind == STMT_BREAK ? loop.break_to : loop.continue_to);
}

static void lower_return(Lowerer *l, Stmt *s) {
    if (s->as.expr) {
        assign_operand(l, (LocalId){0}, lower_expr(l, s->as.expr));
    }
    terminate(l, (Terminator){.kind = TERM_RETURN});
}

static void lower_stmt(Lowerer *l, Stmt *s) {
    switch (s->kind) {
    case STMT_LET: lower_let(l, s); break;
    case STMT_VAR:
        if (s->as.var.value) {
            assign_operand(l, air_local_of(s->as.var.local), lower_expr(l, s->as.var.value));
        }
        break;
    case STMT_ASSIGN: lower_assign(l, s); break;
    case STMT_FOR:
        if (s->as.for_.kind == FOR_IN && s->as.for_.expr->kind != EXPR_RANGE) {
            lower_for_list(l, s);
        } else if (s->as.for_.kind == FOR_IN) {
            lower_for_range(l, s);
        } else {
            lower_while(l, s);
        }
        break;
    case STMT_RETURN: lower_return(l, s); break;
    case STMT_BREAK: case STMT_CONTINUE: lower_jump(l, s); break;
    case STMT_EXPR: lower_expr(l, s->as.expr); break;
    }
}

// As a value, a block is worth its last expression statement.
Operand lower_block(Lowerer *l, Block *block, bool as_value) {
    StmtList stmts = block->stmts;
    for (uint32_t i = 0; i < stmts.len; i++) {
        Stmt *s = stmts.items[i];
        if (as_value && i + 1 == stmts.len && s->kind == STMT_EXPR) {
            return lower_expr(l, s->as.expr);
        }
        lower_stmt(l, s);
    }
    return op_unit();
}
