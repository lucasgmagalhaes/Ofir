// Stage 0 compiler entry point.

#include <stdio.h>
#include <string.h>

#define ASTRA_VERSION "0.0.0"

static void print_usage(FILE *out) {
    fputs("usage: astra <command> [arguments]\n"
          "\n"
          "commands:\n"
          "  build <file.at>   compile a program\n"
          "  run <file.at>     compile and run a program\n"
          "  version           print the compiler version\n",
          out);
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
        fprintf(stderr, "error: `%s` is not implemented yet\n", command);
        return 1;
    }
    fprintf(stderr, "error: unknown command `%s`\n\n", command);
    print_usage(stderr);
    return 2;
}
