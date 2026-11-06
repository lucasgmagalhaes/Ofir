// Statements (spec appendix A.5).

#include "syntax/parser_internal.h"

static Stmt *new_stmt(Parser *p, StmtKind kind, Span start) {
    Stmt *s = NEW(p, Stmt);
    s->kind = kind;
    s->span = span_since(p, start);
    return s;
}

static TypeExpr *optional_type(Parser *p) {
    return accept(p, TOK_COLON) ? parse_type(p) : NULL;
}

// `let pattern: T = value` and `let pattern = value else { ... }`.
static Stmt *let_stmt(Parser *p, Span start) {
    Stmt *s = new_stmt(p, STMT_LET, start);
    s->as.let.pattern = parse_pattern(p);
    s->as.let.type = optional_type(p);
    if (expect(p, TOK_EQ, "`=` in `let`")) {
        skip_newlines(p);
        s->as.let.value = parse_expr(p);
    }
    if (accept(p, TOK_KW_ELSE)) {
        s->as.let.else_ = parse_block(p);
    }
    s->span = span_since(p, start);
    return s;
}

static Stmt *var_stmt(Parser *p, Span start) {
    Stmt *s = new_stmt(p, STMT_VAR, start);
    const Token *name = expect(p, TOK_IDENT, "a variable name after `var`");
    s->as.var.name = name ? name->as.symbol : (Symbol){0};
    s->as.var.type = optional_type(p);
    if (accept(p, TOK_EQ)) {
        skip_newlines(p);
        s->as.var.value = parse_expr(p);
    }
    s->span = span_since(p, start);
    return s;
}

// Whether the tokens after `for` are `[mut] name [, name] in`.
static bool at_for_in(const Parser *p) {
    size_t i = peek(p) == TOK_KW_MUT ? 1 : 0;
    if (peek_at(p, i) != TOK_IDENT) {
        return false;
    }
    if (peek_at(p, i + 1) == TOK_COMMA) {
        return peek_at(p, i + 2) == TOK_IDENT && peek_at(p, i + 3) == TOK_KW_IN;
    }
    return peek_at(p, i + 1) == TOK_KW_IN;
}

static Stmt *for_stmt(Parser *p, Span start) {
    Stmt *s = new_stmt(p, STMT_FOR, start);
    if (at(p, TOK_LBRACE)) {
        s->as.for_.kind = FOR_INFINITE;
    } else if (at_for_in(p)) {
        s->as.for_.kind = FOR_IN;
        s->as.for_.mutable = accept(p, TOK_KW_MUT);
        s->as.for_.first = advance(p)->as.symbol;
        if (accept(p, TOK_COMMA)) {
            s->as.for_.second = advance(p)->as.symbol;
        }
        advance(p);   // in
        s->as.for_.expr = parse_expr_no_struct(p);
    } else {
        s->as.for_.kind = FOR_COND;
        s->as.for_.expr = parse_expr_no_struct(p);
    }
    s->as.for_.body = parse_block(p);
    s->span = span_since(p, start);
    return s;
}

static bool at_statement_end(const Parser *p) {
    return at(p, TOK_NEWLINE) || at(p, TOK_RBRACE) || at(p, TOK_EOF);
}

// Recognizes an assignment operator, joining `>` `>` `=` into `>>=`.
// Returns the number of tokens (0 if there is none).
static int assign_op(const Parser *p, Op *op) {
    switch (peek(p)) {
    case TOK_EQ: *op = OP_ASSIGN; return 1;
    case TOK_PLUS_EQ: *op = OP_ADD; return 1;
    case TOK_MINUS_EQ: *op = OP_SUB; return 1;
    case TOK_STAR_EQ: *op = OP_MUL; return 1;
    case TOK_SLASH_EQ: *op = OP_DIV; return 1;
    case TOK_PERCENT_EQ: *op = OP_REM; return 1;
    case TOK_AMP_EQ: *op = OP_AND; return 1;
    case TOK_PIPE_EQ: *op = OP_OR; return 1;
    case TOK_CARET_EQ: *op = OP_XOR; return 1;
    case TOK_SHL_EQ: *op = OP_SHL; return 1;
    case TOK_GT: {
        const Token *a = peek_token(p, 0), *b = peek_token(p, 1), *c = peek_token(p, 2);
        bool joined = b->kind == TOK_GT && c->kind == TOK_EQ && a->span.end == b->span.start &&
                      b->span.end == c->span.start;
        *op = OP_SHR;
        return joined ? 3 : 0;
    }
    default: return 0;
    }
}

// An expression statement, or an assignment if an assignment operator follows.
static Stmt *expr_stmt(Parser *p, Span start) {
    Expr *target = parse_expr(p);
    Op op;
    int n = assign_op(p, &op);
    if (n == 0) {
        Stmt *s = new_stmt(p, STMT_EXPR, start);
        s->as.expr = target;
        return s;
    }
    p->pos += (size_t)n;
    skip_newlines(p);
    Stmt *s = new_stmt(p, STMT_ASSIGN, start);
    s->as.assign.op = op;
    s->as.assign.target = target;
    s->as.assign.value = parse_expr(p);
    s->span = span_since(p, start);
    return s;
}

Stmt *parse_stmt(Parser *p) {
    Span start = peek_token(p, 0)->span;
    switch (peek(p)) {
    case TOK_KW_LET:
    case TOK_KW_CONST: advance(p); return let_stmt(p, start);
    case TOK_KW_VAR: advance(p); return var_stmt(p, start);
    case TOK_KW_FOR: advance(p); return for_stmt(p, start);
    case TOK_KW_BREAK: advance(p); return new_stmt(p, STMT_BREAK, start);
    case TOK_KW_CONTINUE: advance(p); return new_stmt(p, STMT_CONTINUE, start);
    case TOK_KW_RETURN: {
        advance(p);
        Expr *value = at_statement_end(p) ? NULL : parse_expr(p);
        Stmt *s = new_stmt(p, STMT_RETURN, start);
        s->as.expr = value;
        return s;
    }
    default:
        return expr_stmt(p, start);
    }
}
