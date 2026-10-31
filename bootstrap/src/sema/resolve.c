// Name resolution, first pass: the prelude and the top-level declarations
// (docs/design/compiler-architecture.md, 5). Bodies are in resolve_body.c.

#include "sema/resolve_internal.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    const char *name;
    DefKind kind;
    uint32_t value;   // a TYPE_* constant or a Builtin
} PreludeEntry;

static const PreludeEntry PRELUDE[] = {
    {"int", DEF_PRIMITIVE, TID_INT},     {"uint", DEF_PRIMITIVE, TID_U64},
    {"i8", DEF_PRIMITIVE, TID_I8},       {"i16", DEF_PRIMITIVE, TID_I16},
    {"i32", DEF_PRIMITIVE, TID_I32},     {"i64", DEF_PRIMITIVE, TID_I64},
    {"u8", DEF_PRIMITIVE, TID_U8},       {"u16", DEF_PRIMITIVE, TID_U16},
    {"u32", DEF_PRIMITIVE, TID_U32},     {"u64", DEF_PRIMITIVE, TID_U64},
    {"float", DEF_PRIMITIVE, TID_FLOAT}, {"f32", DEF_PRIMITIVE, TID_F32},
    {"f64", DEF_PRIMITIVE, TID_F64},     {"bool", DEF_PRIMITIVE, TID_BOOL},
    {"str", DEF_PRIMITIVE, TID_STR},
    {"print", DEF_BUILTIN, BUILTIN_PRINT}, {"println", DEF_BUILTIN, BUILTIN_PRINTLN},
    {"panic", DEF_BUILTIN, BUILTIN_PANIC}, {"assert", DEF_BUILTIN, BUILTIN_ASSERT},
};

static DefId add_def(Sema *sema, Def def) {
    VEC_PUSH(sema->defs, def);
    return (DefId){(uint32_t)sema->defs.len - 1};
}

void sema_init(Sema *sema, Session *session, Module *module) {
    *sema = (Sema){.session = session, .module = module};
    types_init(&sema->types, &session->arena);
    VEC_PUSH(sema->defs, (Def){0});   // DefId 0 means "none"
    Symbol names[ARRAY_LEN(PRELUDE)];
    for (size_t i = 0; i < ARRAY_LEN(PRELUDE); i++) {
        names[i] = intern(&session->names, sv_from_cstr(PRELUDE[i].name));
    }
    intern(&session->names, SV("self"));   // before the scope tables are sized
    intern(&session->names, SV("_"));
    scope_init(&sema->scope, session->names.count);
    for (size_t i = 0; i < ARRAY_LEN(PRELUDE); i++) {
        Def def = {.kind = PRELUDE[i].kind, .name = names[i], .in_prelude = true};
        if (def.kind == DEF_PRIMITIVE) {
            def.type = type_id(PRELUDE[i].value);
        } else {
            def.builtin = (Builtin)PRELUDE[i].value;
        }
        scope_set_module(&sema->scope, names[i], add_def(sema, def));
    }
}

void sema_free(Sema *sema) {
    types_free(&sema->types);
    VEC_FREE(sema->defs);
    scope_free(&sema->scope);
}

void resolve_error(Resolver *r, const char *code, Span span, const char *format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    diag_error(&r->sema->session->diags, code, span, "%s", message);
}

StrView resolver_name(const Resolver *r, Symbol name) {
    return symbol_name(&r->sema->session->names, name);
}

// Registers a top-level name; a module declaration may hide a prelude name.
static void declare_item(Resolver *r, Def def) {
    DefId existing = scope_module(&r->sema->scope, def.name);
    if (existing.id != 0 && !sema_def(r->sema, existing)->in_prelude) {
        resolve_error(r, "E0201", def.span, "`" SV_FMT "` is defined more than once",
                      SV_ARG(resolver_name(r, def.name)));
        return;
    }
    scope_set_module(&r->sema->scope, def.name, add_def(r->sema, def));
}

static DefKind item_def_kind(ItemKind kind) {
    switch (kind) {
    case ITEM_STRUCT: case ITEM_TUPLE_STRUCT: return DEF_STRUCT;
    case ITEM_ENUM: return DEF_ENUM;
    case ITEM_CONST: return DEF_CONST;
    case ITEM_GLOBAL: return DEF_GLOBAL;
    case ITEM_TYPE_ALIAS: return DEF_TYPE_ALIAS;
    default: return DEF_FN;
    }
}

static void declare_fn(Resolver *r, const Item *item, FnDecl *fn, bool in_module) {
    Def def = {.kind = DEF_FN, .name = fn->name, .span = fn->span, .item = item, .fn = fn};
    if (in_module) {
        declare_item(r, def);
        fn->def = scope_module(&r->sema->scope, fn->name);
    } else {
        fn->def = add_def(r->sema, def);   // methods are found through their type
    }
}

// Methods and fields share one namespace per type.
static void check_members(Resolver *r, const Item *item) {
    FnList methods = item->kind == ITEM_ENUM ? item->as.enum_.methods : item->as.struct_.methods;
    for (uint32_t i = 0; i < methods.len; i++) {
        FnDecl *fn = methods.items[i];
        for (uint32_t j = 0; j < i; j++) {
            if (symbol_eq(methods.items[j]->name, fn->name)) {
                resolve_error(r, "E0201", fn->span, "`" SV_FMT "` is defined more than once",
                              SV_ARG(resolver_name(r, fn->name)));
            }
        }
        declare_fn(r, item, fn, false);
    }
}

static void collect_item(Resolver *r, const Item *item) {
    switch (item->kind) {
    case ITEM_FN:
        declare_fn(r, item, item->as.fn, true);
        break;
    case ITEM_EXTERN:
        for (uint32_t i = 0; i < item->as.extern_.fns.len; i++) {
            declare_fn(r, item, item->as.extern_.fns.items[i], true);
        }
        break;
    case ITEM_IMPORT:
        resolve_error(r, "E0901", item->span, "imports are not supported yet");
        break;
    default:
        declare_item(r, (Def){.kind = item_def_kind(item->kind), .name = item->name,
                              .span = item->span, .item = item});
        if (item->kind == ITEM_STRUCT || item->kind == ITEM_TUPLE_STRUCT ||
            item->kind == ITEM_ENUM) {
            check_members(r, item);
        }
        break;
    }
}

static void resolve_item(Resolver *r, Item *item) {
    switch (item->kind) {
    case ITEM_FN:
        resolve_fn(r, item->as.fn);
        break;
    case ITEM_STRUCT: case ITEM_TUPLE_STRUCT:
        r->owner = item;
        for (uint32_t i = 0; i < item->as.struct_.methods.len; i++) {
            resolve_fn(r, item->as.struct_.methods.items[i]);
        }
        r->owner = NULL;
        for (uint32_t i = 0; i < item->as.struct_.fields.len; i++) {
            Expr *value = item->as.struct_.fields.items[i].default_value;
            if (value) {
                resolve_expr(r, value);
            }
        }
        break;
    case ITEM_ENUM:
        r->owner = item;
        for (uint32_t i = 0; i < item->as.enum_.methods.len; i++) {
            resolve_fn(r, item->as.enum_.methods.items[i]);
        }
        r->owner = NULL;
        break;
    case ITEM_CONST: case ITEM_GLOBAL:
        resolve_expr(r, item->as.const_.value);
        break;
    default:
        break;
    }
}

void resolve_module(Sema *sema) {
    Resolver r = {.sema = sema};
    r.self_name = intern(&sema->session->names, SV("self"));
    r.wildcard = intern(&sema->session->names, SV("_"));
    for (uint32_t i = 0; i < sema->module->items.len; i++) {
        collect_item(&r, sema->module->items.items[i]);
    }
    for (uint32_t i = 0; i < sema->module->items.len; i++) {
        resolve_item(&r, sema->module->items.items[i]);
    }
    VEC_FREE(r.locals);
}
