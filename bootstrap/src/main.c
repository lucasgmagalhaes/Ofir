// Stage 0 compiler entry point.

#include "base/arena.h"
#include "base/intern.h"
#include "base/source.h"
#include "diag/diag.h"
#include "syntax/lexer.h"

#include <stdio.h>
#include <string.h>

#define ASTRA_VERSION "0.0.0"

typedef enum {
    EMIT_NONE,
    EMIT_TOKENS,
} Emit;

typedef struct {
    const char *command;
    const char *input;
    const char *output;
    Emit emit;
    DiagFormat format;
} Options;

static void print_usage(FILE *out) {
    fputs("usage: astra <command> [arguments]\n"
          "\n"
          "commands:\n"
          "  build <file.at>   compile a program\n"
          "  run <file.at>     compile and run a program\n"
          "  version           print the compiler version\n"
          "\n"
          "options:\n"
          "  -o <path>                 output path\n"
          "  --emit=tokens             print the tokens and stop\n"
          "  --message-format=short    one line per diagnostic\n",
          out);
}

static bool parse_options(int argc, char **argv, Options *opts) {
    *opts = (Options){.command = argv[1], .format = DIAG_FORMAT_HUMAN};
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            opts->output = argv[++i];
        } else if (strcmp(arg, "--emit=tokens") == 0) {
            opts->emit = EMIT_TOKENS;
        } else if (strcmp(arg, "--message-format=short") == 0) {
            opts->format = DIAG_FORMAT_SHORT;
        } else if (strcmp(arg, "--message-format=human") == 0) {
            opts->format = DIAG_FORMAT_HUMAN;
        } else if (arg[0] == '-') {
            fprintf(stderr, "error: unknown option `%s`\n", arg);
            return false;
        } else if (!opts->input) {
            opts->input = arg;
        } else {
            fprintf(stderr, "error: unexpected argument `%s`\n", arg);
            return false;
        }
    }
    if (!opts->input) {
        fprintf(stderr, "error: `%s` needs an input file\n", opts->command);
        return false;
    }
    return true;
}

static int build(const Options *opts) {
    Arena arena;
    arena_init(&arena);
    SourceMap map;
    source_map_init(&map, &arena);
    Interner names;
    interner_init(&names, &arena);
    Diagnostics diags;
    diags_init(&diags, &arena);

    int status = 0;
    FileId file;
    if (!source_map_load(&map, opts->input, &file)) {
        fprintf(stderr, "error[E0900]: cannot read `%s`\n", opts->input);
        status = 1;
        goto done;
    }

    TokenList tokens = {0};
    lex(&map, file, &names, &arena, &diags, &tokens);
    if (opts->emit == EMIT_TOKENS) {
        print_tokens(&tokens, &map, &names, stdout);
    }
    VEC_FREE(tokens);

    diags_render(&diags, &map, opts->format, stderr);
    if (diags_has_errors(&diags)) {
        status = 1;
    } else if (opts->emit == EMIT_NONE) {
        fprintf(stderr, "error: `%s` is not implemented yet (only --emit=tokens)\n", opts->command);
        status = 1;
    }

done:
    diags_free(&diags);
    interner_free(&names);
    source_map_free(&map);
    arena_free(&arena);
    return status;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(stderr);
        return 2;
    }
    const char *command = argv[1];
    if (strcmp(command, "version") == 0) {
        printf("astra %s (stage 0)\n", ASTRA_VERSION);
        return 0;
    }
    if (strcmp(command, "help") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (strcmp(command, "build") == 0 || strcmp(command, "run") == 0) {
        Options opts;
        if (!parse_options(argc, argv, &opts)) {
            return 2;
        }
        return build(&opts);
    }
    fprintf(stderr, "error: unknown command `%s`\n\n", command);
    print_usage(stderr);
    return 2;
}
