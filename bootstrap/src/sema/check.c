// Type checking (docs/design/compiler-architecture.md, 6): signatures first,
// then constants and function bodies, one function at a time.

#include "sema/check_internal.h"

#include <stdarg.h>
#include <stdio.h>

void check_error(Checker *c, const char *code, Span span, const char *format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    diag_error(&c->sema->session->diags, code, span, "%s", message);
}

TypeId unsupported(Checker *c, Span span, const char *what) {
    check_error(c, "E0901", span, "%s not supported yet", what);
    return type_id(TID_ERROR);
}

static StrView adt_name(const void *context, uint32_t def) {
    Sema *sema = (Sema *)context;
    return symbol_name(&sema->session->names, sema_def(sema, (DefId){def})->name);
}

const char *type_name(Checker *c, TypeId type) {
    static StrBuf buf;
    strbuf_clear(&buf);
    ty_text(&buf, c->types, type, adt_name, c->sema);
    return buf.data;
}

static void check_fn(Checker *c, FnDecl *fn, TypeId self_type) {
    if (fn->type_params.len > 0) {
        unsupported(c, fn->span, "generic functions are");
        return;
    }
    const Ty *sig = type_get(c->types, fn_signature(c, fn));
    c->fn = fn;
    c->ret = sig->inner;
    c->self_type = self_type;
    c->local_types = ARENA_ARRAY(&c->sema->session->arena, TypeId, fn->locals.len + 1);
    fn->local_types = c->local_types;
    if (fn->has_self) {
        c->local_types[0] = self_type;
    }
    for (uint32_t i = 0; i < fn->params.len; i++) {
        c->local_types[fn->params.items[i].local.id] = sig->args[i];
    }
    if (!fn->body) {
        return;
    }
    TypeId body = check_block(c, fn->body, NO_TYPE, false);
    bool needs_value = c->ret.id != TID_UNIT && c->ret.id != TID_ERROR;
    if (needs_value && body.id != TID_NEVER) {
        check_error(c, "E0303", fn->span,
                    "function may reach its end without returning a value");
    }
}

static void check_methods(Checker *c, const Item *item, FnList methods) {
    DefId owner = scope_module(&c->sema->scope, item->name);
    if (item->kind != ITEM_ENUM && item->as.struct_.type_params.len > 0) {
        unsupported(c, item->span, "generic types are");
        return;
    }
    TypeId self_type = type_intern(c->types, (Ty){.kind = TY_ADT, .data = owner.id});
    for (uint32_t i = 0; i < methods.len; i++) {
        check_fn(c, methods.items[i], self_type);
    }
}

static void check_item(Checker *c, Item *item) {
    switch (item->kind) {
    case ITEM_FN: check_fn(c, item->as.fn, NO_TYPE); break;
    case ITEM_EXTERN:
        for (uint32_t i = 0; i < item->as.extern_.fns.len; i++) {
            fn_signature(c, item->as.extern_.fns.items[i]);
        }
        break;
    case ITEM_STRUCT: case ITEM_TUPLE_STRUCT:
        check_methods(c, item, item->as.struct_.methods);
        break;
    case ITEM_ENUM: check_methods(c, item, item->as.enum_.methods); break;
    case ITEM_CONST: case ITEM_GLOBAL:
        check_const(c, sema_def(c->sema, scope_module(&c->sema->scope, item->name)));
        break;
    case ITEM_TYPE_ALIAS: case ITEM_IMPORT:
        break;
    }
}

void check_module(Sema *sema) {
    Checker c = {.sema = sema, .types = &sema->types, .self_type = NO_TYPE};
    for (uint32_t i = 0; i < sema->module->items.len; i++) {
        check_item(&c, sema->module->items.items[i]);
    }
    VEC_FREE(c.loops);
}
