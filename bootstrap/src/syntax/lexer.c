// Lexer main loop, comments, words and punctuation. Literals are in
// literals.c. Port of the stage 1 prototype (compiler/syntax/lexer.at).

#include "syntax/lexer_internal.h"

static const struct {
    const char *word;
    TokenKind kind;
} keywords[] = {
    {"as", TOK_KW_AS},         {"break", TOK_KW_BREAK},   {"const", TOK_KW_CONST},
    {"continue", TOK_KW_CONTINUE}, {"copy", TOK_KW_COPY}, {"dyn", TOK_KW_DYN},
    {"else", TOK_KW_ELSE},     {"enum", TOK_KW_ENUM},     {"extern", TOK_KW_EXTERN},
    {"false", TOK_KW_FALSE},   {"fn", TOK_KW_FN},         {"for", TOK_KW_FOR},
    {"if", TOK_KW_IF},         {"import", TOK_KW_IMPORT}, {"in", TOK_KW_IN},
    {"interface", TOK_KW_INTERFACE}, {"let", TOK_KW_LET}, {"match", TOK_KW_MATCH},
    {"mut", TOK_KW_MUT},       {"pub", TOK_KW_PUB},       {"return", TOK_KW_RETURN},
    {"self", TOK_KW_SELF_VALUE}, {"Self", TOK_KW_SELF_TYPE}, {"struct", TOK_KW_STRUCT},
    {"true", TOK_KW_TRUE},     {"unsafe", TOK_KW_UNSAFE}, {"var", TOK_KW_VAR},
};

Token *emit(Lexer *lx, TokenKind kind, uint32_t start) {
    Token token = {.kind = kind, .span = span_from(lx, start)};
    VEC_PUSH(*lx->out, token);
    return &VEC_LAST(*lx->out);
}

// Newlines are tokens except directly inside `(...)` and `[...]` (spec 1.3).
static bool newlines_matter(const Lexer *lx) {
    return lx->brackets.len == 0 || VEC_LAST(lx->brackets) == '{';
}

// ---------------------------------------------------------------------------
// Comments

static StrView trimmed_copy(Lexer *lx, uint32_t start, uint32_t end) {
    while (start < end && (lx->src[start] == ' ' || lx->src[start] == '\t')) {
        start++;
    }
    while (end > start && (lx->src[end - 1] == ' ' || lx->src[end - 1] == '\r')) {
        end--;
    }
    return (StrView){arena_strndup(&lx->session->arena, lx->src + start, end - start), end - start};
}

// Skips a `//` comment; `///` and `//!` become doc comment tokens.
static void line_comment(Lexer *lx) {
    uint32_t start = lx->pos;
    bool doc = peek(lx, 2) == '/' && peek(lx, 3) != '/';
    bool module_doc = peek(lx, 2) == '!';
    while (!at_end(lx) && lx->src[lx->pos] != '\n') {
        lx->pos++;
    }
    if (doc || module_doc) {
        Token *t = emit(lx, doc ? TOK_DOC_COMMENT : TOK_MODULE_DOC, start);
        t->as.text = trimmed_copy(lx, start + 3, lx->pos);
    }
}

// Skips `/* ... */`. Returns true if the comment contained a newline.
static bool block_comment(Lexer *lx) {
    uint32_t start = lx->pos;
    lx->pos += 2;
    bool newline = false;
    for (;;) {
        if (at_end(lx)) {
            diag_error(&lx->session->diags, "E0003", span_from(lx, start),
                       "unterminated block comment");
            return newline;
        }
        if (lx->src[lx->pos] == '*' && peek(lx, 1) == '/') {
            lx->pos += 2;
            return newline;
        }
        newline |= lx->src[lx->pos] == '\n';
        lx->pos++;
    }
}

// ---------------------------------------------------------------------------
// Words

static void word(Lexer *lx) {
    uint32_t start = lx->pos;
    while (!at_end(lx) && (is_letter(lx->src[lx->pos]) || is_digit(lx->src[lx->pos]))) {
        lx->pos++;
    }
    StrView text = {lx->src + start, lx->pos - start};
    for (size_t i = 0; i < ARRAY_LEN(keywords); i++) {
        if (sv_eq_cstr(text, keywords[i].word)) {
            emit(lx, keywords[i].kind, start);
            return;
        }
    }
    emit(lx, TOK_IDENT, start)->as.symbol = intern(&lx->session->names, text);
}

// ---------------------------------------------------------------------------
// Punctuation

// Consumes `expected` if it is the next byte.
static bool eat(Lexer *lx, char expected) {
    if (peek(lx, 0) == expected) {
        lx->pos++;
        return true;
    }
    return false;
}

static TokenKind pick(Lexer *lx, char expected, TokenKind if_match, TokenKind otherwise) {
    return eat(lx, expected) ? if_match : otherwise;
}

static TokenKind open_bracket(Lexer *lx, char bracket, TokenKind kind) {
    VEC_PUSH(lx->brackets, bracket);
    return kind;
}

// Mismatched brackets are reported by the parser; the lexer only keeps its
// stack balanced.
static TokenKind close_bracket(Lexer *lx, TokenKind kind) {
    if (lx->brackets.len) {
        lx->brackets.len--;
    }
    return kind;
}

static bool closes_interpolation(Lexer *lx) {
    if (lx->interpolations.len > 0 && VEC_LAST(lx->interpolations) == lx->brackets.len) {
        lx->interpolations.len--;
        return true;
    }
    return false;
}

static TokenKind invalid_character(Lexer *lx, uint32_t start) {
    // Skip the rest of a multi-byte UTF-8 character so it is reported once.
    while (!at_end(lx) && ((unsigned char)lx->src[lx->pos] & 0xc0) == 0x80) {
        lx->pos++;
    }
    diag_error(&lx->session->diags, "E0001", span_from(lx, start), "unexpected character");
    return TOK_INVALID;
}

// `>` is always a single token: the parser joins adjacent ones (spec 1.6).
// check-sizes: table
static TokenKind punct_kind(Lexer *lx, char c, uint32_t start) {
    switch (c) {
    case '(': return open_bracket(lx, '(', TOK_LPAREN);
    case '[': return open_bracket(lx, '[', TOK_LBRACKET);
    case '{': return open_bracket(lx, '{', TOK_LBRACE);
    case ')': return close_bracket(lx, TOK_RPAREN);
    case ']': return close_bracket(lx, TOK_RBRACKET);
    case '}': return close_bracket(lx, TOK_RBRACE);
    case ',': return TOK_COMMA;
    case ':': return TOK_COLON;
    case ';': return TOK_SEMICOLON;
    case '@': return TOK_AT;
    case '>': return TOK_GT;
    case '?': return pick(lx, '?', TOK_QUESTION_QUESTION, TOK_QUESTION);
    case '.': return eat(lx, '.') ? pick(lx, '=', TOK_DOT_DOT_EQ, TOK_DOT_DOT) : TOK_DOT;
    case '=': return eat(lx, '>') ? TOK_FAT_ARROW : pick(lx, '=', TOK_EQ_EQ, TOK_EQ);
    case '+': return pick(lx, '=', TOK_PLUS_EQ, TOK_PLUS);
    case '-': return pick(lx, '=', TOK_MINUS_EQ, TOK_MINUS);
    case '*': return pick(lx, '=', TOK_STAR_EQ, TOK_STAR);
    case '/': return pick(lx, '=', TOK_SLASH_EQ, TOK_SLASH);
    case '%': return pick(lx, '=', TOK_PERCENT_EQ, TOK_PERCENT);
    case '^': return pick(lx, '=', TOK_CARET_EQ, TOK_CARET);
    case '!': return pick(lx, '=', TOK_NE, TOK_BANG);
    case '&': return eat(lx, '&') ? TOK_AND_AND : pick(lx, '=', TOK_AMP_EQ, TOK_AMP);
    case '|': return eat(lx, '|') ? TOK_OR_OR : pick(lx, '=', TOK_PIPE_EQ, TOK_PIPE);
    case '<':
        return eat(lx, '<') ? pick(lx, '=', TOK_SHL_EQ, TOK_SHL) : pick(lx, '=', TOK_LE, TOK_LT);
    default: return invalid_character(lx, start);
    }
}

static void punct(Lexer *lx) {
    uint32_t start = lx->pos;
    char c = lx->src[lx->pos++];
    if (c == '}' && closes_interpolation(lx)) {
        lex_string_part(lx, start, false);
        return;
    }
    emit(lx, punct_kind(lx, c, start), start);
}

// ---------------------------------------------------------------------------
// Main loop

static void token_at(Lexer *lx) {
    uint32_t start = lx->pos;
    char c = lx->src[start];
    if (c == 'b' && peek(lx, 1) == '\'') {
        lex_byte_literal(lx);
    } else if (is_letter(c)) {
        word(lx);
    } else if (is_digit(c)) {
        lex_number(lx);
    } else if (c == '"') {
        lx->pos++;
        lex_string_part(lx, start, true);
    } else {
        punct(lx);
    }
}

// Handles whitespace and comments at the current position. Returns false
// if the current byte starts a token.
static bool skip_trivia(Lexer *lx) {
    char c = lx->src[lx->pos];
    uint32_t start = lx->pos;
    if (c == ' ' || c == '\t' || c == '\r') {
        lx->pos++;
    } else if (c == '\n') {
        lx->pos++;
        if (newlines_matter(lx)) {
            emit(lx, TOK_NEWLINE, start);
        }
    } else if (c == '/' && peek(lx, 1) == '/') {
        line_comment(lx);
    } else if (c == '/' && peek(lx, 1) == '*') {
        if (block_comment(lx) && newlines_matter(lx)) {
            emit(lx, TOK_NEWLINE, start);
        }
    } else {
        return false;
    }
    return true;
}

void lex(Session *session, FileId file, TokenList *out) {
    const SourceFile *source = source_file(&session->sources, file);
    Lexer lx = {
        .session = session,
        .src = source->text.ptr,
        .len = (uint32_t)source->text.len,
        .file = file,
        .out = out,
    };
    while (!at_end(&lx)) {
        if (!skip_trivia(&lx)) {
            token_at(&lx);
        }
    }
    emit(&lx, TOK_EOF, lx.pos);
    VEC_FREE(lx.brackets);
    VEC_FREE(lx.interpolations);
}
