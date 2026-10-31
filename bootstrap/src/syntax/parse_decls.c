// Functions, structs and enums (spec appendix A.2 and A.3).

#include "syntax/parser_internal.h"

// ---------------------------------------------------------------------------
// Functions

static Param param(Parser *p) {
    Param param = {.span = peek_token(p, 0)->span};
    const Token *name = expect(p, TOK_IDENT, "a parameter name");
    param.name = name ? name->as.symbol : (Symbol){0};
    expect(p, TOK_COLON, "`:` and the parameter type");
    param.mode = parse_mode(p);
    param.type = parse_type(p);
    if (accept(p, TOK_EQ)) {
        param.default_value = parse_expr(p);
    }
    param.span = span_since(p, param.span);
    return param;
}

// `[mut | move] self` as the first parameter of a method.
static bool self_param(Parser *p, FnDecl *fn) {
    size_t i = at(p, TOK_KW_MUT) || at_word(p, p->words.move) ? 1 : 0;
    if (peek_at(p, i) != TOK_KW_SELF_VALUE) {
        return false;
    }
    fn->self_mode = parse_mode(p);
    advance(p);   // self
    fn->has_self = true;
    return true;
}

static void params(Parser *p, FnDecl *fn) {
    expect(p, TOK_LPAREN, "`(` and the parameters");
    Vec(Param) items = {0};
    bool first = self_param(p, fn);
    if (!first || accept(p, TOK_COMMA)) {
        while (at(p, TOK_IDENT)) {
            VEC_PUSH(items, param(p));
            if (!accept(p, TOK_COMMA)) {
                break;
            }
        }
    }
    expect(p, TOK_RPAREN, "`)` to close the parameters");
    TO_LIST(p, items, fn->params);
}

// After `fn`. `with_body` is false in `extern` blocks.
FnDecl *parse_fn(Parser *p, ItemHeader header, bool with_body) {
    FnDecl *fn = NEW(p, FnDecl);
    fn->attributes = header.attributes;
    fn->is_pub = header.is_pub;
    const Token *name = expect(p, TOK_IDENT, "a function name");
    fn->name = name ? name->as.symbol : (Symbol){0};
    fn->type_params = parse_type_params(p);
    params(p, fn);
    if (!at(p, TOK_LBRACE) && can_start_type(p)) {
        fn->ret = parse_type(p);
    }
    if (at_word(p, p->words.where)) {
        syntax_error(p, peek_token(p, 0)->span, "`where` clauses are not supported by stage 0");
    }
    if (with_body) {
        fn->body = parse_block(p);
    }
    fn->span = span_since(p, header.span);
    return fn;
}

// ---------------------------------------------------------------------------
// Struct and enum bodies

static bool at_method(const Parser *p) {
    size_t i = 0;
    while (peek_at(p, i) == TOK_KW_PUB || peek_at(p, i) == TOK_KW_UNSAFE) {
        i++;
    }
    return peek_at(p, i) == TOK_KW_FN || peek_at(p, i) == TOK_AT;
}

static FnDecl *method(Parser *p) {
    ItemHeader header = parse_item_header(p);
    accept(p, TOK_KW_UNSAFE);
    expect(p, TOK_KW_FN, "`fn`");
    return parse_fn(p, header, true);
}

static void member_end(Parser *p) {
    bool separated = accept_separator(p);
    skip_newlines(p);
    if (!separated && !at(p, TOK_RBRACE)) {
        unexpected(p, "a new line between members");
    }
    if (p->recovering) {
        synchronize(p);
    }
}

static FieldDecl field(Parser *p) {
    ItemHeader header = parse_item_header(p);
    FieldDecl f = {.attributes = header.attributes, .is_pub = header.is_pub, .span = header.span};
    const Token *name = expect(p, TOK_IDENT, "a field name");
    f.name = name ? name->as.symbol : (Symbol){0};
    expect(p, TOK_COLON, "`:` and the field type");
    f.type = parse_type(p);
    if (accept(p, TOK_EQ)) {
        f.default_value = parse_expr(p);
    }
    f.span = span_since(p, f.span);
    return f;
}

// `{ fields... methods... }` after the struct name.
static void struct_body(Parser *p, Item *item) {
    Vec(FieldDecl) fields = {0};
    Vec(FnDecl *) methods = {0};
    expect(p, TOK_LBRACE, "`{` and the fields");
    skip_newlines(p);
    while (!at(p, TOK_RBRACE) && !at(p, TOK_EOF)) {
        if (at_method(p)) {
            VEC_PUSH(methods, method(p));
        } else {
            VEC_PUSH(fields, field(p));
        }
        member_end(p);
    }
    expect(p, TOK_RBRACE, "`}` to close the struct");
    TO_LIST(p, fields, item->as.struct_.fields);
    TO_LIST(p, methods, item->as.struct_.methods);
}

// `(T, U)` and an optional `{ methods }` after a tuple struct name.
static void tuple_struct_body(Parser *p, Item *item) {
    Vec(FieldDecl) fields = {0};
    advance(p);   // (
    while (!at(p, TOK_RPAREN) && !at(p, TOK_EOF)) {
        ItemHeader header = parse_item_header(p);
        FieldDecl f = {.is_pub = header.is_pub, .span = header.span, .type = parse_type(p)};
        VEC_PUSH(fields, f);
        if (!accept(p, TOK_COMMA)) {
            break;
        }
    }
    expect(p, TOK_RPAREN, "`)` to close the fields");
    Vec(FnDecl *) methods = {0};
    if (accept(p, TOK_LBRACE)) {
        skip_newlines(p);
        while (!at(p, TOK_RBRACE) && !at(p, TOK_EOF)) {
            VEC_PUSH(methods, method(p));
            member_end(p);
        }
        expect(p, TOK_RBRACE, "`}` to close the methods");
    }
    TO_LIST(p, fields, item->as.struct_.fields);
    TO_LIST(p, methods, item->as.struct_.methods);
}

// After `struct` (and `copy` or `ref`).
void parse_struct(Parser *p, Item *item) {
    const Token *name = expect(p, TOK_IDENT, "a struct name");
    item->name = name ? name->as.symbol : (Symbol){0};
    item->as.struct_.type_params = parse_type_params(p);
    if (at(p, TOK_COLON)) {
        syntax_error(p, peek_token(p, 0)->span,
                     "interface assertions are not supported by stage 0");
        return;
    }
    if (at(p, TOK_LPAREN)) {
        item->kind = ITEM_TUPLE_STRUCT;
        tuple_struct_body(p, item);
    } else {
        struct_body(p, item);
    }
}

static Variant variant(Parser *p) {
    Variant v = {.span = peek_token(p, 0)->span};
    const Token *name = expect(p, TOK_IDENT, "a variant name");
    v.name = name ? name->as.symbol : (Symbol){0};
    Vec(VariantField) fields = {0};
    if (accept(p, TOK_LPAREN)) {
        v.has_fields = true;
        while (!at(p, TOK_RPAREN) && !at(p, TOK_EOF)) {
            VariantField f = {0};
            if (at(p, TOK_IDENT) && peek_at(p, 1) == TOK_COLON) {
                f.name = advance(p)->as.symbol;
                advance(p);
            }
            f.type = parse_type(p);
            VEC_PUSH(fields, f);
            if (!accept(p, TOK_COMMA)) {
                break;
            }
        }
        expect(p, TOK_RPAREN, "`)` to close the variant fields");
    }
    if (accept(p, TOK_EQ)) {
        v.discriminant = parse_expr(p);
    }
    TO_LIST(p, fields, v.fields);
    v.span = span_since(p, v.span);
    return v;
}

// After `enum` (and `copy`).
void parse_enum(Parser *p, Item *item) {
    const Token *name = expect(p, TOK_IDENT, "an enum name");
    item->name = name ? name->as.symbol : (Symbol){0};
    item->as.enum_.type_params = parse_type_params(p);
    Vec(Variant) variants = {0};
    Vec(FnDecl *) methods = {0};
    expect(p, TOK_LBRACE, "`{` and the variants");
    skip_newlines(p);
    while (!at(p, TOK_RBRACE) && !at(p, TOK_EOF)) {
        if (at_method(p)) {
            VEC_PUSH(methods, method(p));
        } else {
            VEC_PUSH(variants, variant(p));
        }
        member_end(p);
    }
    expect(p, TOK_RBRACE, "`}` to close the enum");
    TO_LIST(p, variants, item->as.enum_.variants);
    TO_LIST(p, methods, item->as.enum_.methods);
}
