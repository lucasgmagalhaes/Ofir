#include "session.h"

void session_init(Session *session) {
    arena_init(&session->arena);
    source_map_init(&session->sources, &session->arena);
    interner_init(&session->names, &session->arena);
    diags_init(&session->diags, &session->arena);
}

void session_free(Session *session) {
    diags_free(&session->diags);
    interner_free(&session->names);
    source_map_free(&session->sources);
    arena_free(&session->arena);
}
