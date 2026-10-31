// Loading the modules of a program (spec 7.1 and 7.2): the root file, then
// every module it imports, transitively. `import a.b` is the file
// `<root dir>/a/b.at` or `<root dir>/a/b/mod.at`; `import std.x` is in the
// standard library. When `a.b` names a declaration rather than a module
// (`import std.collections.Map`), its module `a` is loaded. Imports that
// match no file are reported by the resolver (E0204).

#include "driver_internal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    Pipeline *p;
    Vec(ProgramModule) modules;
    char root_dir[PATH_SIZE];
    char std_dir[PATH_SIZE];
    bool has_std;
} Loader;

static StrView dotted_name(Loader *ld, SymbolList path, uint32_t count) {
    StrBuf buf = {0};
    for (uint32_t i = 0; i < count; i++) {
        strbuf_append_cstr(&buf, i ? "." : "");
        strbuf_append(&buf, symbol_name(&ld->p->session->names, path.items[i]));
    }
    char *text = arena_strndup(&ld->p->session->arena, buf.data ? buf.data : "", buf.len);
    StrView name = {text, buf.len};
    strbuf_free(&buf);
    return name;
}

static bool is_loaded(const Loader *ld, StrView name) {
    for (size_t i = 0; i < ld->modules.len; i++) {
        if (sv_eq(ld->modules.data[i].name, name)) {
            return true;
        }
    }
    return false;
}

// The file of module `name`, if it exists.
static bool module_file(Loader *ld, StrView name, char out[PATH_SIZE]) {
    bool in_std = name.len >= 4 && memcmp(name.ptr, "std.", 4) == 0;
    if (in_std && !ld->has_std) {
        return false;
    }
    const char *dir = in_std ? ld->std_dir : ld->root_dir;
    StrView rest = in_std ? (StrView){name.ptr + 4, name.len - 4} : name;
    char relative[PATH_SIZE];
    size_t len = rest.len < PATH_SIZE - 1 ? rest.len : PATH_SIZE - 1;
    for (size_t i = 0; i < len; i++) {
        relative[i] = rest.ptr[i] == '.' ? '/' : rest.ptr[i];
    }
    relative[len] = '\0';
    static const char *const FORMS[] = {"%s/%s.at", "%s/%s/mod.at"};
    for (size_t i = 0; i < ARRAY_LEN(FORMS); i++) {
        int n = snprintf(out, PATH_SIZE, FORMS[i], dir, relative);
        if (n > 0 && n < PATH_SIZE && access(out, R_OK) == 0) {
            return true;
        }
    }
    return false;
}

static void load(Loader *ld, StrView name, const char *path) {
    FileId file;
    if (!source_map_load(&ld->p->session->sources, path, &file)) {
        return;
    }
    Next next;
    Module *module = parse_file(ld->p, file, false, &next);
    if (module) {
        VEC_PUSH(ld->modules, ((ProgramModule){.module = module, .name = name}));
    }
}

static void load_import(Loader *ld, const Item *item) {
    SymbolList path = item->as.import.path;
    for (uint32_t count = path.len; count >= 1 && count + 1 >= path.len; count--) {
        StrView name = dotted_name(ld, path, count);   // the module, then its parent
        char file[PATH_SIZE];
        if (is_loaded(ld, name)) {
            return;
        }
        if (module_file(ld, name, file)) {
            load(ld, name, file);
            return;
        }
    }
}

// The module name of the root file: its name without the directory and `.at`.
static StrView root_name(Loader *ld, const char *input) {
    const char *base = strrchr(input, '/');
    base = base ? base + 1 : input;
    size_t len = strlen(base);
    if (len > 3 && strcmp(base + len - 3, ".at") == 0) {
        len -= 3;
    }
    return (StrView){arena_strndup(&ld->p->session->arena, base, len), len};
}

Program load_program(Pipeline *p, Module *root) {
    Loader ld = {.p = p};
    const char *slash = strrchr(p->opts->input, '/');
    int dir_len = slash ? (int)(slash - p->opts->input) : 1;
    snprintf(ld.root_dir, sizeof ld.root_dir, "%.*s", dir_len, slash ? p->opts->input : ".");
    static const char *const STD[] = {"../../std", "../lib/astra/std"};
    ld.has_std = find_installed(p->opts->argv0, "ASTRA_STD", STD, ARRAY_LEN(STD), ld.std_dir);
    VEC_PUSH(ld.modules, ((ProgramModule){.module = root, .name = root_name(&ld, p->opts->input)}));
    for (size_t m = 0; m < ld.modules.len; m++) {   // grows while it is walked
        ItemList items = ld.modules.data[m].module->items;
        for (uint32_t i = 0; i < items.len; i++) {
            if (items.items[i]->kind == ITEM_IMPORT) {
                load_import(&ld, items.items[i]);
            }
        }
    }
    Program program = {.len = (uint32_t)ld.modules.len};
    program.items = ARENA_ARRAY(&p->session->arena, ProgramModule, ld.modules.len);
    memcpy(program.items, ld.modules.data, ld.modules.len * sizeof(ProgramModule));
    VEC_FREE(ld.modules);
    return program;
}
