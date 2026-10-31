// Semantic analysis state: the prelude and the modules of the program.

#include "sema/sema.h"

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

void sema_init(Sema *sema, Session *session, Program program) {
    *sema = (Sema){.session = session, .program = program, .module = program.items[0].module};
    types_init(&sema->types, &session->arena);
    VEC_PUSH(sema->defs, (Def){0});   // DefId 0 means "none"
    Symbol names[ARRAY_LEN(PRELUDE)];
    for (size_t i = 0; i < ARRAY_LEN(PRELUDE); i++) {
        names[i] = intern(&session->names, sv_from_cstr(PRELUDE[i].name));
    }
    intern(&session->names, SV("self"));   // before the scope tables are sized
    intern(&session->names, SV("_"));
    scope_init(&sema->scope, session->names.count, program.len);
    for (size_t i = 0; i < ARRAY_LEN(PRELUDE); i++) {
        Def def = {.kind = PRELUDE[i].kind, .name = names[i], .in_prelude = true, .is_pub = true};
        if (def.kind == DEF_PRIMITIVE) {
            def.type = type_id(PRELUDE[i].value);
        } else {
            def.builtin = (Builtin)PRELUDE[i].value;
        }
        DefId id = add_def(sema, def);
        for (uint32_t m = 0; m < program.len; m++) {   // visible in every module
            sema->scope.tables[m][names[i].id] = id.id;
        }
    }
}

uint32_t sema_enter(Sema *sema, uint32_t index) {
    uint32_t previous = sema->current;
    sema->current = index;
    sema->module = sema->program.items[index].module;
    scope_use_module(&sema->scope, index);
    return previous;
}

void sema_free(Sema *sema) {
    types_free(&sema->types);
    VEC_FREE(sema->defs);
    scope_free(&sema->scope);
}

