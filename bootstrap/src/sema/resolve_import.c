// Imports and module-qualified names (spec 7.2 and 7.3).
//
// `import parser.lexer` binds `lexer` to the module; `import
// std.collections.Map` binds `Map` to that module's declaration. An
// expression `lexer.tokenize` is rewritten into a direct reference to the
// declaration, so later phases never see module names.

#include "sema/resolve_internal.h"
#include "sema/suggest.h"

#include <string.h>

static StrView module_name(Resolver *r, uint32_t index) {
    return r->sema->program.items[index].name;
}

// The module whose dotted name is the first `count` segments of `path`.
static int64_t find_module(Resolver *r, SymbolList path, uint32_t count) {
    char dotted[512];
    size_t len = 0;
    for (uint32_t i = 0; i < count; i++) {
        StrView part = resolver_name(r, path.items[i]);
        if (len + part.len + 1 >= sizeof dotted) {
            return -1;
        }
        if (i > 0) {
            dotted[len++] = '.';
        }
        memcpy(dotted + len, part.ptr, part.len);
        len += part.len;
    }
    for (uint32_t m = 0; m < r->sema->program.len; m++) {
        if (sv_eq(module_name(r, m), (StrView){dotted, len})) {
            return m;
        }
    }
    return -1;
}

DefId module_member(Resolver *r, uint32_t index, Symbol name, Span span) {
    DefId id = scope_module_in(&r->sema->scope, index, name);
    const Def *def = id.id ? sema_def(r->sema, id) : NULL;
    if (!def || def->in_prelude || def->module != index) {   // no re-exports
        resolve_error(r, "E0200", span, "cannot find `" SV_FMT "` in module `" SV_FMT "`",
                      SV_ARG(resolver_name(r, name)), SV_ARG(module_name(r, index)));
        suggest_member(r->sema, index, name);
        return (DefId){0};
    }
    if (!def->is_pub) {
        resolve_error(r, "E0202", span, "`" SV_FMT "` is private to module `" SV_FMT "`",
                      SV_ARG(resolver_name(r, name)), SV_ARG(module_name(r, index)));
    }
    return id;
}

// Whether two bindings name the same thing: the same declaration, or the
// same module imported twice (a test file may repeat its module's imports).
static bool same_target(Resolver *r, DefId a, DefId b) {
    const Def *x = sema_def(r->sema, a), *y = sema_def(r->sema, b);
    return a.id == b.id || (x->kind == DEF_MODULE && y->kind == DEF_MODULE &&
                            x->target == y->target);
}

static void bind(Resolver *r, Symbol name, DefId id, Span span) {
    DefId existing = scope_module(&r->sema->scope, name);
    if (existing.id != 0 && same_target(r, existing, id)) {
        return;
    }
    if (existing.id != 0 && !sema_def(r->sema, existing)->in_prelude) {
        resolve_error(r, "E0201", span, "`" SV_FMT "` is defined more than once",
                      SV_ARG(resolver_name(r, name)));
        return;
    }
    scope_set_module(&r->sema->scope, name, id);
}

static void bind_import(Resolver *r, const Item *item);

void resolve_import(Resolver *r, const Item *item) {
    bind_import(r, item);
    if (tracing(r)) {
        SymbolList path = item->as.import.path;
        Symbol name = item->as.import.alias.id ? item->as.import.alias : path.items[path.len - 1];
        Res res = {RES_DEF, scope_module(&r->sema->scope, name).id};
        trace_use(r, "import", name, item->span, res);
    }
}

static void bind_import(Resolver *r, const Item *item) {
    SymbolList path = item->as.import.path;
    Symbol last = path.items[path.len - 1];
    Symbol name = item->as.import.alias.id ? item->as.import.alias : last;
    VEC_PUSH(r->sema->imports, ((ImportUse){r->sema->current, name, item->span, false}));
    int64_t module = find_module(r, path, path.len);
    if (module >= 0) {   // a module
        VEC_PUSH(r->sema->defs, ((Def){.kind = DEF_MODULE, .name = name, .span = item->span,
                                       .module = r->sema->current, .target = (uint32_t)module}));
        bind(r, name, (DefId){(uint32_t)r->sema->defs.len - 1}, item->span);
        return;
    }
    int64_t owner = path.len > 1 ? find_module(r, path, path.len - 1) : -1;
    if (owner < 0) {
        StrBuf dotted = {0};
        for (uint32_t i = 0; i < path.len; i++) {
            strbuf_append_cstr(&dotted, i ? "." : "");
            strbuf_append(&dotted, resolver_name(r, path.items[i]));
        }
        resolve_error(r, "E0204", item->span, "cannot find module `%s`", dotted.data);
        strbuf_free(&dotted);
        return;
    }
    DefId member = module_member(r, (uint32_t)owner, last, item->span);   // a declaration
    if (member.id) {
        bind(r, name, member, item->span);
    }
}

void resolve_qualified(Resolver *r, Expr *e) {
    Expr *base = e->as.field.base;
    if (base->kind != EXPR_IDENT || base->as.ident.res.kind != RES_DEF) {
        return;
    }
    const Def *def = sema_def(r->sema, (DefId){base->as.ident.res.index});
    if (def->kind != DEF_MODULE) {
        return;
    }
    Symbol name = e->as.field.name;
    DefId member = module_member(r, def->target, name, e->span);
    e->kind = EXPR_IDENT;
    e->as.ident.name = name;
    e->as.ident.res = member.id ? (Res){RES_DEF, member.id} : (Res){RES_ERROR, 0};
    trace_use(r, "member", name, e->span, e->as.ident.res);
}

// `std.core` and its submodules, `std.core.box`, ... form the prelude.
static bool in_core(Resolver *r, uint32_t m) {
    StrView name = module_name(r, m);
    return sv_eq(name, SV("std.core")) ||
           (name.len > 9 && memcmp(name.ptr, "std.core.", 9) == 0);
}

// Makes the public declarations of the prelude module `core` visible in
// every other module, below the module's own declarations (spec 7.4).
static void export_core(Resolver *r, uint32_t core) {
    Scope *scope = &r->sema->scope;
    for (uint32_t name = 1; name < scope->symbol_count; name++) {
        uint32_t id = scope->tables[core][name];
        const Def *def = id ? sema_def(r->sema, (DefId){id}) : NULL;
        if (!def || !def->is_pub || def->module != core) {
            continue;
        }
        for (uint32_t m = 0; m < r->sema->program.len; m++) {
            uint32_t own = scope->tables[m][name];
            if (m != core && (own == 0 || sema_def(r->sema, (DefId){own})->in_prelude)) {
                scope->tables[m][name] = id;
            }
        }
    }
}

void import_prelude(Resolver *r) {
    for (uint32_t m = 0; m < r->sema->program.len; m++) {
        if (in_core(r, m)) {
            export_core(r, m);
        }
    }
}
