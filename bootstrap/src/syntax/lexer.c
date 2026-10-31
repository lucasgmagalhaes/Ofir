// Port of the stage 1 lexer prototype (compiler/syntax/lexer.at).

#include "syntax/lexer.h"

#include <stdlib.h>
#include <string.h>

static const char *const token_texts[] = {
#define X(name, text) text,
    TOKEN_KINDS(X)
#undef X
};

const char *token_kind_text(TokenKind kind) {
    return token_texts[kind];
}

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

typedef struct {
    const char *src;
    uint32_t len;
    uint32_t pos;
    FileId file;
    Interner *names;
    Arena *arena;
    Diagnostics *diags;
    TokenList *out;
    Vec(char) brackets;            // open `(`, `[` and `{`, innermost last
    Vec(uint32_t) interpolations;  // brackets.len when each interpolation opened
} Lexer;

// ---------------------------------------------------------------------------
// Helpers

static char peek(const Lexer *lx, uint32_t offset) {
    uint32_t i = lx->pos + offset;
    return i < lx->len ? lx->src[i] : '\0';
}

static bool at_end(const Lexer *lx) {
    return lx->pos >= lx->len;
}

static Span span_from(const Lexer *lx, uint32_t start) {
    return (Span){lx->file, start, lx->pos};
}

static Token *emit(Lexer *lx, TokenKind kind, uint32_t start) {
    Token token = {.kind = kind, .span = span_from(lx, start)};
    VEC_PUSH(*lx->out, token);
    return &VEC_LAST(*lx->out);
}

static bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// Value of a digit in any base up to 16, or 99 for any other byte.
static int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 99;
}

// Newlines are tokens except directly inside `(...)` and `[...]` (spec 1.3).
static bool newlines_matter(const Lexer *lx) {
    return lx->brackets.len == 0 || VEC_LAST(lx->brackets) == '{';
}

static StrView copy_text(Lexer *lx, const StrBuf *buf) {
    StrView text = strbuf_view(buf);
    return (StrView){arena_strndup(lx->arena, text.ptr, text.len), text.len};
}

// Appends the UTF-8 encoding of a Unicode scalar value.
static void push_utf8(StrBuf *buf, uint32_t c) {
    if (c < 0x80) {
        strbuf_push(buf, (char)c);
    } else if (c < 0x800) {
        strbuf_push(buf, (char)(0xc0 | (c >> 6)));
        strbuf_push(buf, (char)(0x80 | (c & 0x3f)));
    } else if (c < 0x10000) {
        strbuf_push(buf, (char)(0xe0 | (c >> 12)));
        strbuf_push(buf, (char)(0x80 | ((c >> 6) & 0x3f)));
        strbuf_push(buf, (char)(0x80 | (c & 0x3f)));
    } else {
        strbuf_push(buf, (char)(0xf0 | (c >> 18)));
        strbuf_push(buf, (char)(0x80 | ((c >> 12) & 0x3f)));
        strbuf_push(buf, (char)(0x80 | ((c >> 6) & 0x3f)));
        strbuf_push(buf, (char)(0x80 | (c & 0x3f)));
    }
}

// ---------------------------------------------------------------------------
// Comments

static void line_comment(Lexer *lx) {
    uint32_t start = lx->pos;
    TokenKind kind = TOK_EOF;   // TOK_EOF means "ordinary comment"
    if (peek(lx, 2) == '/' && peek(lx, 3) != '/') {
        kind = TOK_DOC_COMMENT;
    } else if (peek(lx, 2) == '!') {
        kind = TOK_MODULE_DOC;
    }
    while (!at_end(lx) && lx->src[lx->pos] != '\n') {
        lx->pos++;
    }
    if (kind == TOK_EOF) {
        return;
    }
    uint32_t text_start = start + 3, text_end = lx->pos;
    while (text_start < text_end && (lx->src[text_start] == ' ' || lx->src[text_start] == '\t')) {
        text_start++;
    }
    while (text_end > text_start && (lx->src[text_end - 1] == ' ' || lx->src[text_end - 1] == '\r')) {
        text_end--;
    }
    Token *t = emit(lx, kind, start);
    t->as.text = (StrView){arena_strndup(lx->arena, lx->src + text_start, text_end - text_start),
                           text_end - text_start};
}

// Skips `/* ... */`. Returns true if the comment contained a newline.
static bool block_comment(Lexer *lx) {
    uint32_t start = lx->pos;
    lx->pos += 2;
    bool newline = false;
    for (;;) {
        if (at_end(lx)) {
            diag_error(lx->diags, "E0003", span_from(lx, start), "unterminated block comment");
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
// Words and numbers

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
    emit(lx, TOK_IDENT, start)->as.symbol = intern(lx->names, text);
}

static void skip_digits(Lexer *lx) {
    while (!at_end(lx) && (is_digit(lx->src[lx->pos]) || lx->src[lx->pos] == '_')) {
        lx->pos++;
    }
}

// `1.5` and `2e10` are floats; `0..10` is a range and `x.0` a tuple field.
static bool at_fraction_or_exponent(const Lexer *lx) {
    char c = peek(lx, 0);
    if (c == '.') {
        return is_digit(peek(lx, 1));
    }
    if (c == 'e' || c == 'E') {
        char next = peek(lx, 1);
        return is_digit(next) || ((next == '+' || next == '-') && is_digit(peek(lx, 2)));
    }
    return false;
}

static void float_rest(Lexer *lx, uint32_t start) {
    if (peek(lx, 0) == '.') {
        lx->pos++;
        skip_digits(lx);
    }
    if (peek(lx, 0) == 'e' || peek(lx, 0) == 'E') {
        lx->pos++;
        if (peek(lx, 0) == '+' || peek(lx, 0) == '-') {
            lx->pos++;
        }
        skip_digits(lx);
    }
    StrBuf digits = {0};
    for (uint32_t i = start; i < lx->pos; i++) {
        if (lx->src[i] != '_') {
            strbuf_push(&digits, lx->src[i]);
        }
    }
    Token *t = emit(lx, TOK_FLOAT, start);
    t->as.float_value = strtod(digits.data, NULL);
    strbuf_free(&digits);
}

static void number(Lexer *lx) {
    uint32_t start = lx->pos;
    int base = 10;
    if (lx->src[start] == '0') {
        switch (peek(lx, 1)) {
        case 'x': base = 16; break;
        case 'o': base = 8; break;
        case 'b': base = 2; break;
        default: break;
        }
    }
    if (base != 10) {
        lx->pos += 2;
    }

    uint64_t value = 0;
    bool overflow = false;
    while (!at_end(lx)) {
        char c = lx->src[lx->pos];
        if (c == '_') {
            lx->pos++;
            continue;
        }
        int digit = digit_value(c);
        if (digit >= base) {
            break;
        }
        if (value > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base) {
            overflow = true;
        }
        value = value * (uint64_t)base + (uint64_t)digit;
        lx->pos++;
    }

    if (base == 10 && at_fraction_or_exponent(lx)) {
        float_rest(lx, start);
        return;
    }
    if (is_letter(peek(lx, 0))) {
        diag_error(lx->diags, "E0001", (Span){lx->file, lx->pos, lx->pos + 1},
                   "unexpected character after number");
    }
    if (overflow) {
        diag_error(lx->diags, "E0005", span_from(lx, start), "integer literal is too large");
        value = 0;
    }
    emit(lx, TOK_INT, start)->as.int_value = value;
}

// ---------------------------------------------------------------------------
// Strings and bytes

// Reads exactly `count` hex digits; returns -1 if they are missing.
static long hex_digits(Lexer *lx, int count) {
    long value = 0;
    for (int i = 0; i < count; i++) {
        int d = digit_value(peek(lx, 0));
        if (d >= 16) {
            return -1;
        }
        value = value * 16 + d;
        lx->pos++;
    }
    return value;
}

// Reads `{H...}` after `\u`; returns -1 if malformed.
static long unicode_escape(Lexer *lx) {
    if (peek(lx, 0) != '{') {
        return -1;
    }
    lx->pos++;
    long value = 0;
    int digits = 0;
    while (digit_value(peek(lx, 0)) < 16) {
        if (digits == 6) {
            return -1;
        }
        value = value * 16 + digit_value(peek(lx, 0));
        digits++;
        lx->pos++;
    }
    if (digits == 0 || peek(lx, 0) != '}') {
        return -1;
    }
    lx->pos++;
    return value;
}

static void escape(Lexer *lx, StrBuf *out) {
    uint32_t start = lx->pos;
    lx->pos++;   // the backslash
    char c = peek(lx, 0);
    if (!at_end(lx)) {
        lx->pos++;
    }
    switch (c) {
    case 'n': strbuf_push(out, '\n'); return;
    case 't': strbuf_push(out, '\t'); return;
    case 'r': strbuf_push(out, '\r'); return;
    case '0': strbuf_push(out, '\0'); return;
    case '\\': case '"': case '{': case '}': strbuf_push(out, c); return;
    case 'x': {
        long value = hex_digits(lx, 2);
        if (value < 0 || value > 0x7f) {
            diag_error(lx->diags, "E0004", span_from(lx, start),
                       "`\\x` escapes in strings must be between \\x00 and \\x7f");
        } else {
            strbuf_push(out, (char)value);
        }
        return;
    }
    case 'u': {
        long value = unicode_escape(lx);
        if (value < 0 || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
            diag_error(lx->diags, "E0004", span_from(lx, start), "invalid unicode escape");
        } else {
            push_utf8(out, (uint32_t)value);
        }
        return;
    }
    default:
        diag_error(lx->diags, "E0004", span_from(lx, start), "invalid escape sequence");
        return;
    }
}

static bool is_string_special(char c) {
    return c == '"' || c == '\\' || c == '{' || c == '}' || c == '\n';
}

// Lexes string text up to the closing quote or the next interpolation.
// `opening` is true right after the opening quote and false after the `}`
// that ends an interpolation; with the stopping character it selects
// STR, STR_START, STR_MID or STR_END.
static void string_part(Lexer *lx, uint32_t start, bool opening) {
    StrBuf text = {0};
    TokenKind kind;
    for (;;) {
        if (at_end(lx) || lx->src[lx->pos] == '\n') {
            diag_error(lx->diags, "E0002", span_from(lx, start), "unterminated string literal");
            kind = opening ? TOK_STR : TOK_STR_END;
            break;
        }
        char c = lx->src[lx->pos];
        if (c == '"') {
            lx->pos++;
            kind = opening ? TOK_STR : TOK_STR_END;
            break;
        }
        if (c == '{') {
            lx->pos++;
            VEC_PUSH(lx->interpolations, (uint32_t)lx->brackets.len);
            kind = opening ? TOK_STR_START : TOK_STR_MID;
            break;
        }
        if (c == '}') {
            diag_error(lx->diags, "E0006", (Span){lx->file, lx->pos, lx->pos + 1},
                       "unescaped `}` in string literal; write `\\}`");
            lx->pos++;
            continue;
        }
        if (c == '\\') {
            escape(lx, &text);
            continue;
        }
        // Copy a run of ordinary bytes. Runs stop only at ASCII bytes, so
        // each run of the (valid UTF-8) source is itself valid UTF-8.
        uint32_t run = lx->pos;
        while (!at_end(lx) && !is_string_special(lx->src[lx->pos])) {
            lx->pos++;
        }
        strbuf_append(&text, (StrView){lx->src + run, lx->pos - run});
    }
    emit(lx, kind, start)->as.text = copy_text(lx, &text);
    strbuf_free(&text);
}

// `b'a'`, `b'\n'`, `b'\xff'`: any byte value, unlike `\x` in strings.
static void byte_literal(Lexer *lx) {
    uint32_t start = lx->pos;
    lx->pos += 2;   // b'
    long value = (unsigned char)peek(lx, 0);
    if (!at_end(lx)) {
        lx->pos++;
    }
    if (value == '\\') {
        char e = peek(lx, 0);
        if (!at_end(lx)) {
            lx->pos++;
        }
        switch (e) {
        case 'n': value = '\n'; break;
        case 't': value = '\t'; break;
        case 'r': value = '\r'; break;
        case '0': value = 0; break;
        case '\\': value = '\\'; break;
        case '\'': value = '\''; break;
        case 'x': value = hex_digits(lx, 2); break;
        default: value = -1; break;
        }
    }
    if (value < 0 || peek(lx, 0) != '\'') {
        diag_error(lx->diags, "E0004", span_from(lx, start), "invalid byte literal");
        // Resynchronize after the closing quote on this line, if any.
        while (!at_end(lx) && lx->src[lx->pos] != '\'' && lx->src[lx->pos] != '\n') {
            lx->pos++;
        }
        if (peek(lx, 0) == '\'') {
            lx->pos++;
        }
        emit(lx, TOK_BYTE, start)->as.byte_value = 0;
        return;
    }
    lx->pos++;
    emit(lx, TOK_BYTE, start)->as.byte_value = (uint8_t)value;
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

static bool closes_interpolation(Lexer *lx) {
    if (lx->interpolations.len > 0 && VEC_LAST(lx->interpolations) == lx->brackets.len) {
        lx->interpolations.len--;
        return true;
    }
    return false;
}

static void punct(Lexer *lx) {
    uint32_t start = lx->pos;
    char c = lx->src[lx->pos++];
    TokenKind kind;
    switch (c) {
    case '(': VEC_PUSH(lx->brackets, '('); kind = TOK_LPAREN; break;
    case '[': VEC_PUSH(lx->brackets, '['); kind = TOK_LBRACKET; break;
    case '{': VEC_PUSH(lx->brackets, '{'); kind = TOK_LBRACE; break;
    case ')': if (lx->brackets.len) lx->brackets.len--; kind = TOK_RPAREN; break;
    case ']': if (lx->brackets.len) lx->brackets.len--; kind = TOK_RBRACKET; break;
    case '}':
        if (closes_interpolation(lx)) {
            string_part(lx, start, false);
            return;
        }
        if (lx->brackets.len) lx->brackets.len--;
        kind = TOK_RBRACE;
        break;
    case ',': kind = TOK_COMMA; break;
    case ':': kind = TOK_COLON; break;
    case ';': kind = TOK_SEMICOLON; break;
    case '@': kind = TOK_AT; break;
    case '>': kind = TOK_GT; break;
    case '?': kind = pick(lx, '?', TOK_QUESTION_QUESTION, TOK_QUESTION); break;
    case '.':
        kind = eat(lx, '.') ? pick(lx, '=', TOK_DOT_DOT_EQ, TOK_DOT_DOT) : TOK_DOT;
        break;
    case '=': kind = eat(lx, '>') ? TOK_FAT_ARROW : pick(lx, '=', TOK_EQ_EQ, TOK_EQ); break;
    case '+': kind = pick(lx, '=', TOK_PLUS_EQ, TOK_PLUS); break;
    case '-': kind = pick(lx, '=', TOK_MINUS_EQ, TOK_MINUS); break;
    case '*': kind = pick(lx, '=', TOK_STAR_EQ, TOK_STAR); break;
    case '/': kind = pick(lx, '=', TOK_SLASH_EQ, TOK_SLASH); break;
    case '%': kind = pick(lx, '=', TOK_PERCENT_EQ, TOK_PERCENT); break;
    case '^': kind = pick(lx, '=', TOK_CARET_EQ, TOK_CARET); break;
    case '!': kind = pick(lx, '=', TOK_NE, TOK_BANG); break;
    case '&': kind = eat(lx, '&') ? TOK_AND_AND : pick(lx, '=', TOK_AMP_EQ, TOK_AMP); break;
    case '|': kind = eat(lx, '|') ? TOK_OR_OR : pick(lx, '=', TOK_PIPE_EQ, TOK_PIPE); break;
    case '<':
        kind = eat(lx, '<') ? pick(lx, '=', TOK_SHL_EQ, TOK_SHL) : pick(lx, '=', TOK_LE, TOK_LT);
        break;
    default:
        // Skip the rest of a multi-byte UTF-8 character so it is reported once.
        while (!at_end(lx) && ((unsigned char)lx->src[lx->pos] & 0xc0) == 0x80) {
            lx->pos++;
        }
        diag_error(lx->diags, "E0001", span_from(lx, start), "unexpected character");
        kind = TOK_INVALID;
        break;
    }
    emit(lx, kind, start);
}

// ---------------------------------------------------------------------------
// Main loop

static void token_at(Lexer *lx) {
    uint32_t start = lx->pos;
    char c = lx->src[start];
    if (c == 'b' && peek(lx, 1) == '\'') {
        byte_literal(lx);
    } else if (is_letter(c)) {
        word(lx);
    } else if (is_digit(c)) {
        number(lx);
    } else if (c == '"') {
        lx->pos++;
        string_part(lx, start, true);
    } else {
        punct(lx);
    }
}

void lex(const SourceMap *map, FileId file, Interner *names, Arena *arena, Diagnostics *diags,
         TokenList *out) {
    const SourceFile *source = source_file(map, file);
    Lexer lx = {
        .src = source->text.ptr,
        .len = (uint32_t)source->text.len,
        .file = file,
        .names = names,
        .arena = arena,
        .diags = diags,
        .out = out,
    };
    while (!at_end(&lx)) {
        char c = lx.src[lx.pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lx.pos++;
        } else if (c == '\n') {
            uint32_t start = lx.pos++;
            if (newlines_matter(&lx)) {
                emit(&lx, TOK_NEWLINE, start);
            }
        } else if (c == '/' && peek(&lx, 1) == '/') {
            line_comment(&lx);
        } else if (c == '/' && peek(&lx, 1) == '*') {
            uint32_t start = lx.pos;
            if (block_comment(&lx) && newlines_matter(&lx)) {
                emit(&lx, TOK_NEWLINE, start);
            }
        } else {
            token_at(&lx);
        }
    }
    emit(&lx, TOK_EOF, lx.pos);
    VEC_FREE(lx.brackets);
    VEC_FREE(lx.interpolations);
}

// ---------------------------------------------------------------------------
// Printing

static void print_escaped(StrView text, FILE *out) {
    fputc('"', out);
    for (size_t i = 0; i < text.len; i++) {
        unsigned char c = (unsigned char)text.ptr[i];
        switch (c) {
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        case '\\': fputs("\\\\", out); break;
        case '"': fputs("\\\"", out); break;
        case '{': fputs("\\{", out); break;
        case '}': fputs("\\}", out); break;
        default:
            if (c < 0x20 || c == 0x7f) {
                fprintf(out, "\\x%02x", c);
            } else {
                fputc((char)c, out);
            }
        }
    }
    fputc('"', out);
}

void print_tokens(const TokenList *tokens, const SourceMap *map, const Interner *names, FILE *out) {
    for (size_t i = 0; i < tokens->len; i++) {
        const Token *t = &tokens->data[i];
        LineCol lc = source_line_col(map, t->span.file, t->span.start);
        fprintf(out, "%u:%u %s", lc.line, lc.column, token_kind_text(t->kind));
        switch (t->kind) {
        case TOK_IDENT:
            fprintf(out, " " SV_FMT, SV_ARG(symbol_name(names, t->as.symbol)));
            break;
        case TOK_INT:
            fprintf(out, " %llu", (unsigned long long)t->as.int_value);
            break;
        case TOK_FLOAT: {
            // Shortest representation that reads back to the same value.
            char buf[32];
            for (int precision = 1; precision <= 17; precision++) {
                snprintf(buf, sizeof buf, "%.*g", precision, t->as.float_value);
                if (strtod(buf, NULL) == t->as.float_value) {
                    break;
                }
            }
            fprintf(out, " %s", buf);
            break;
        }
        case TOK_BYTE:
            fprintf(out, " %u", t->as.byte_value);
            break;
        case TOK_STR: case TOK_STR_START: case TOK_STR_MID: case TOK_STR_END:
        case TOK_DOC_COMMENT: case TOK_MODULE_DOC:
            fputc(' ', out);
            print_escaped(t->as.text, out);
            break;
        default:
            break;
        }
        fputc('\n', out);
    }
}
