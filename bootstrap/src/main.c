// Stage 0 compiler entry point: command line parsing.

#include "driver.h"

#include <stdio.h>
#include <string.h>

#define ASTRA_VERSION "0.1.0"

static void print_usage(FILE *out) {
    fputs("usage: astra <command> [arguments]\n"
          "\n"
          "commands:\n"
          "  build <file.at>   compile a program\n"
          "  run <file.at>     compile and run a program\n"
          "  test <file.at>    compile and run the `@test` functions\n"
          "  version           print the compiler version\n"
          "\n"
          "options:\n"
          "  -o <path>                 output path\n"
          "  --emit=tokens             print the tokens and stop\n"
          "  --emit=ast                print the syntax tree and stop\n"
          "  --emit=air                print the intermediate representation and stop\n"
          "  --emit=llvm               write <output>.ll and stop\n"
          "  --release                 optimize the program\n"
          "  --deny-warnings           treat warnings as errors\n"
          "  --message-format=short    one line per diagnostic\n"
          "  --stop-after=PHASE       run the phases up to lex, parse or check, without output\n"
          "  --time-passes             report the time spent in each phase\n",
          out);
}

// Applies one option; returns false if it is unknown. `i` may advance past
// an option's argument.
static bool parse_option(int argc, char **argv, int *i, Options *opts) {
    const char *arg = argv[*i];
    if (strcmp(arg, "-o") == 0 && *i + 1 < argc) {
        opts->output = argv[++*i];
    } else if (strcmp(arg, "--emit=tokens") == 0) {
        opts->emit = EMIT_TOKENS;
    } else if (strcmp(arg, "--emit=ast") == 0) {
        opts->emit = EMIT_AST;
    } else if (strcmp(arg, "--emit=air") == 0) {
        opts->emit = EMIT_AIR;
    } else if (strcmp(arg, "--emit=llvm") == 0) {
        opts->emit = EMIT_LLVM;
    } else if (strcmp(arg, "--deny-warnings") == 0) {
        opts->deny_warnings = true;
    } else if (strcmp(arg, "--release") == 0) {
        opts->release = true;
    } else if (strcmp(arg, "--message-format=short") == 0) {
        opts->format = DIAG_FORMAT_SHORT;
    } else if (strcmp(arg, "--message-format=human") == 0) {
        opts->format = DIAG_FORMAT_HUMAN;
    } else if (strcmp(arg, "--stop-after=lex") == 0) {
        opts->stop_after = STOP_AFTER_LEX;
    } else if (strcmp(arg, "--stop-after=parse") == 0) {
        opts->stop_after = STOP_AFTER_PARSE;
    } else if (strcmp(arg, "--stop-after=check") == 0) {
        opts->stop_after = STOP_AFTER_CHECK;
    } else if (strcmp(arg, "--time-passes") == 0) {
        opts->time_passes = true;
    } else {
        return false;
    }
    return true;
}

static bool parse_options(int argc, char **argv, Options *opts) {
    *opts = (Options){.command = argv[1], .format = DIAG_FORMAT_HUMAN, .argv0 = argv[0]};
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!parse_option(argc, argv, &i, opts)) {
                fprintf(stderr, "error: unknown option `%s`\n", argv[i]);
                return false;
            }
        } else if (!opts->input) {
            opts->input = argv[i];
        } else {
            fprintf(stderr, "error: unexpected argument `%s`\n", argv[i]);
            return false;
        }
    }
    if (!opts->input) {
        fprintf(stderr, "error: `%s` needs an input file\n", opts->command);
        return false;
    }
    return true;
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
    if (strcmp(command, "build") == 0 || strcmp(command, "run") == 0 ||
        strcmp(command, "test") == 0) {
        Options opts;
        return parse_options(argc, argv, &opts) ? driver_build(&opts) : 2;
    }
    fprintf(stderr, "error: unknown command `%s`\n\n", command);
    print_usage(stderr);
    return 2;
}
