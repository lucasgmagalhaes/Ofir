#include "sema/suggest.h"

#define MAX_NAME 128   // longer names get no suggestion

// Edit distance between `a` and `b`, both at most MAX_NAME bytes: the
// insertions, deletions, substitutions and swaps of two adjacent bytes that
// turn one into the other (optimal string alignment).
static size_t edit_distance(StrView a, StrView b) {
    size_t rows[3][MAX_NAME + 1];
    size_t *before = rows[0], *above = rows[1], *row = rows[2];
    for (size_t j = 0; j <= b.len; j++) {
        above[j] = j;
    }
    for (size_t i = 1; i <= a.len; i++) {
        row[0] = i;
        for (size_t j = 1; j <= b.len; j++) {
            size_t best = above[j - 1] + (a.ptr[i - 1] == b.ptr[j - 1] ? 0 : 1);
            if (above[j] + 1 < best) {
                best = above[j] + 1;
            }
            if (row[j - 1] + 1 < best) {
                best = row[j - 1] + 1;
            }
            if (i > 1 && j > 1 && a.ptr[i - 1] == b.ptr[j - 2] && a.ptr[i - 2] == b.ptr[j - 1] &&
                before[j - 2] + 1 < best) {
                best = before[j - 2] + 1;
            }
            row[j] = best;
        }
        size_t *oldest = before;
        before = above;
        above = row;
        row = oldest;
    }
    return above[b.len];
}

Suggestion suggestion_start(StrView name) {
    return (Suggestion){name, {0}, {0}, 0};
}

void suggestion_offer(Suggestion *s, Symbol candidate, StrView text) {
    size_t limit = s->name.len / 3;
    if (limit == 0 || s->name.len > MAX_NAME || text.len > MAX_NAME) {
        return;
    }
    size_t gap = text.len > s->name.len ? text.len - s->name.len : s->name.len - text.len;
    if (gap > limit) {
        return;
    }
    size_t distance = edit_distance(s->name, text);
    if (distance == 0 || distance > limit) {
        return;
    }
    bool closer = s->best.id == 0 || distance < s->distance ||
                  (distance == s->distance && candidate.id < s->best.id);
    if (closer) {
        s->best = candidate;
        s->best_name = text;
        s->distance = distance;
    }
}

void suggestion_report(const Suggestion *s, Diagnostics *diags) {
    if (s->best.id != 0) {
        diag_help(diags, "did you mean `" SV_FMT "`?", SV_ARG(s->best_name));
    }
}

static bool is_type(DefKind kind) {
    return kind == DEF_PRIMITIVE || kind == DEF_STRUCT || kind == DEF_ENUM ||
           kind == DEF_TYPE_ALIAS;
}

void suggest_name(Sema *sema, Symbol name, SuggestScope where) {
    const Interner *names = &sema->session->names;
    const Scope *scope = &sema->scope;
    Suggestion s = suggestion_start(symbol_name(names, name));
    for (uint32_t i = 1; i < scope->symbol_count; i++) {
        uint32_t def = scope->module[i];
        bool visible = where == SUGGEST_VALUE && scope->local[i] != 0;
        if (!visible && def != 0) {
            visible = where != SUGGEST_TYPE || is_type(sema_def(sema, (DefId){def})->kind);
        }
        if (visible) {
            suggestion_offer(&s, (Symbol){i}, symbol_name(names, (Symbol){i}));
        }
    }
    suggestion_report(&s, &sema->session->diags);
}

void suggest_member(Sema *sema, uint32_t index, Symbol name) {
    const Interner *names = &sema->session->names;
    const Scope *scope = &sema->scope;
    Suggestion s = suggestion_start(symbol_name(names, name));
    for (uint32_t i = 1; i < scope->symbol_count; i++) {
        uint32_t id = scope->tables[index][i];
        const Def *def = id ? sema_def(sema, (DefId){id}) : NULL;
        if (def && !def->in_prelude && def->module == index && def->is_pub &&
            def->kind != DEF_MODULE) {
            suggestion_offer(&s, (Symbol){i}, symbol_name(names, (Symbol){i}));
        }
    }
    suggestion_report(&s, &sema->session->diags);
}

void suggest_field(Sema *sema, const StructInfo *info, Symbol name) {
    const Interner *names = &sema->session->names;
    Suggestion s = suggestion_start(symbol_name(names, name));
    for (uint32_t i = 0; info && !info->is_tuple && i < info->count; i++) {
        suggestion_offer(&s, info->names[i], symbol_name(names, info->names[i]));
    }
    suggestion_report(&s, &sema->session->diags);
}

void suggest_method(Sema *sema, const Item *item, Symbol name) {
    const Interner *names = &sema->session->names;
    Suggestion s = suggestion_start(symbol_name(names, name));
    FnList methods = item->kind == ITEM_ENUM ? item->as.enum_.methods : item->as.struct_.methods;
    for (uint32_t i = 0; i < methods.len; i++) {
        suggestion_offer(&s, methods.items[i]->name, symbol_name(names, methods.items[i]->name));
    }
    suggestion_report(&s, &sema->session->diags);
}
