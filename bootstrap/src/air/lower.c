// Lowering: functions and the helpers that build blocks and statements.

#include "air/lower_internal.h"

LocalId new_temp(Lowerer *l, TypeId type) {
    return air_new_local(l->body, type, (Symbol){0}, (Span){0});
}

void emit_assign(Lowerer *l, LocalId dest, Rvalue rvalue) {
    AirStmt s = {.kind = AIR_ASSIGN, .dest = dest, .has_dest = true};
    s.as.rvalue = rvalue;
    air_push(l->body, l->block, s);
}

LocalId emit_temp(Lowerer *l, TypeId type, Rvalue rvalue) {
    LocalId temp = new_temp(l, type);
    emit_assign(l, temp, rvalue);
    return temp;
}

void emit_runtime(Lowerer *l, RuntimeFn fn, const Operand *args, uint32_t count, LocalId *dest) {
    AirStmt s = {.kind = AIR_CALL, .has_dest = dest != NULL};
    if (dest) {
        s.dest = *dest;
    }
    s.as.call.runtime = true;
    s.as.call.rt = fn;
    s.as.call.arg_count = count;
    s.as.call.args = ARENA_ARRAY(&l->sema->session->arena, Operand, count + 1);
    for (uint32_t i = 0; i < count; i++) {
        s.as.call.args[i] = args[i];
    }
    air_push(l->body, l->block, s);
}

// Continues in `block`. The block left behind was opened by `terminate` and
// nothing jumps to it: it ends as unreachable.
void switch_to(Lowerer *l, BlockId block) {
    AirBlock *current = air_block(l->body, l->block);
    if (current->term.kind == TERM_NONE) {
        current->term.kind = TERM_UNREACHABLE;
    }
    l->block = block;
}

void terminate(Lowerer *l, Terminator term) {
    air_terminate(l->body, l->block, term);
    l->block = air_new_block(l->body);   // unreachable until something jumps here
}

void emit_goto(Lowerer *l, BlockId target) {
    terminate(l, (Terminator){.kind = TERM_GOTO, .target = target});
}

void emit_assert(Lowerer *l, Operand fails, PanicKind panic, Span span) {
    BlockId next = air_new_block(l->body);
    air_terminate(l->body, l->block, (Terminator){.kind = TERM_ASSERT, .cond = fails,
                                                  .target = next, .panic = panic, .span = span});
    l->block = next;
}

LocalId to_local(Lowerer *l, Operand operand) {
    if (operand.kind == OPERAND_COPY) {
        return operand.as.local;
    }
    return emit_temp(l, operand.type, (Rvalue){.kind = RV_USE, .a = operand});
}

// `mut` parameters are passed by reference (roadmap 2.12); until then they
// are rejected rather than silently passed by value.
static bool parameters_supported(Lowerer *l, FnDecl *fn) {
    for (uint32_t i = 0; i < fn->params.len; i++) {
        if (fn->params.items[i].mode == MODE_MUT) {
            diag_error(&l->sema->session->diags, "E0901", fn->params.items[i].span,
                       "`mut` parameters are not supported yet");
            return false;
        }
    }
    return true;
}

static void lower_fn(Lowerer *l, FnDecl *fn, BodySink sink, void *context) {
    if (!parameters_supported(l, fn)) {
        return;
    }
    Def *def = sema_def(l->sema, fn->def);
    const Ty *sig = type_get(l->types, def->type);
    AirBody body = {.def = fn->def, .name = fn->name};
    body.is_main = sv_eq(symbol_name(&l->sema->session->names, fn->name), SV("main"));
    body.param_count = fn->params.len + (fn->has_self ? 1 : 0);
    l->body = &body;
    air_new_local(&body, sig->inner, (Symbol){0}, fn->span);   // the return value
    for (uint32_t i = 0; i < fn->locals.len; i++) {
        const Local *local = &fn->locals.items[i];
        air_new_local(&body, fn->local_types[i], local->name, local->span);
    }
    l->block = air_new_block(&body);
    lower_block(l, fn->body, false);
    // Falling off the end returns (); other functions were checked to return.
    TermKind end = sig->inner.id == TID_UNIT ? TERM_RETURN : TERM_UNREACHABLE;
    air_terminate(&body, l->block, (Terminator){.kind = end});
    sink(context, &body);
    air_body_free(&body);
}

void lower_module(Sema *sema, BodySink sink, void *context) {
    Lowerer l = {.sema = sema, .types = &sema->types};
    DefId main_fn = scope_module(&sema->scope, intern(&sema->session->names, SV("main")));
    Def *main_def = main_fn.id ? sema_def(sema, main_fn) : NULL;
    if (!main_def || main_def->kind != DEF_FN || !main_def->fn->body) {
        diag_error(&sema->session->diags, "E0203", (Span){.file = sema->module->file},
                   "the program has no `main` function");
    } else if (main_def->fn->params.len > 0 || main_def->fn->ret) {
        diag_error(&sema->session->diags, "E0203", main_def->span,
                   "`main` takes no parameters and returns nothing: `fn main() { ... }`");
    }
    for (uint32_t i = 0; i < sema->module->items.len; i++) {
        Item *item = sema->module->items.items[i];
        bool has_methods = (item->kind == ITEM_STRUCT || item->kind == ITEM_TUPLE_STRUCT ||
                            item->kind == ITEM_ENUM);
        if (item->kind == ITEM_FN && item->as.fn->body) {
            lower_fn(&l, item->as.fn, sink, context);
        } else if (has_methods || item->kind == ITEM_GLOBAL || item->kind == ITEM_EXTERN) {
            diag_error(&sema->session->diags, "E0901", item->span,
                       "compiling this declaration is not supported yet");
        }
    }
    VEC_FREE(l.loops);
}
