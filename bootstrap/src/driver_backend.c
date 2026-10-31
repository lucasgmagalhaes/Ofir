// The back end: lowering, LLVM IR, clang (docs/design/toolchain.md, 4).

#define _XOPEN_SOURCE 700   // realpath

#include "driver_internal.h"

#include "air/air_print.h"
#include "air/lower.h"
#include "codegen/llvm.h"

#include <errno.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

// The executable path: `-o`, else the input's name without `.at` (for
// `run`, in a temporary directory).
static bool output_path(const Options *opts, char out[PATH_SIZE]) {
    if (opts->output) {
        return snprintf(out, PATH_SIZE, "%s", opts->output) < PATH_SIZE;
    }
    const char *base = strrchr(opts->input, '/');
    base = base ? base + 1 : opts->input;
    size_t len = strlen(base);
    if (len > 3 && strcmp(base + len - 3, ".at") == 0) {
        len -= 3;
    }
    if (strcmp(opts->command, "run") != 0) {
        return snprintf(out, PATH_SIZE, "%.*s", (int)len, base) < PATH_SIZE;
    }
    const char *tmp = getenv("TMPDIR");
    char dir[PATH_SIZE];
    snprintf(dir, sizeof dir, "%s/astra-run-XXXXXX", tmp && tmp[0] ? tmp : "/tmp");
    if (!mkdtemp(dir)) {
        return false;
    }
    return snprintf(out, PATH_SIZE, "%s/%.*s", dir, (int)len, base) < PATH_SIZE;
}

// The runtime library: $ASTRA_RUNTIME, else next to the compiler
// (build/stage0/astra -> build/libastra_rt.a), else in ../lib/astra.
static bool runtime_path(const char *argv0, char out[PATH_SIZE]) {
    const char *env = getenv("ASTRA_RUNTIME");
    if (env && env[0]) {
        return snprintf(out, PATH_SIZE, "%s", env) < PATH_SIZE;
    }
    char self[PATH_SIZE];
    ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);   // Linux
    if (n > 0) {
        self[n] = '\0';
    } else if (!realpath(argv0, self)) {   // macOS: the path the compiler was started with
        return false;
    }
    char *slash = strrchr(self, '/');
    *slash = '\0';
    static const char *const CANDIDATES[] = {"../libastra_rt.a", "../lib/astra/libastra_rt.a"};
    for (size_t i = 0; i < ARRAY_LEN(CANDIDATES); i++) {
        int n_out = snprintf(out, PATH_SIZE, "%s/%s", self, CANDIDATES[i]);
        if (n_out > 0 && n_out < PATH_SIZE && access(out, R_OK) == 0) {
            return true;
        }
    }
    return false;
}

static int spawn_wait(char *const argv[]) {
    pid_t pid;
    int err = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (err != 0) {
        fprintf(stderr, "error[E0902]: cannot run `%s`: %s\n", argv[0], strerror(err));
        return -1;
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

int run_program(const char *exe) {
    char *argv[] = {(char *)exe, NULL};
    int status = spawn_wait(argv);
    return status < 0 ? 1 : status;
}

static Next link_program(const Options *opts, const char *ll, const char *exe) {
    char runtime[PATH_SIZE];
    if (!runtime_path(opts->argv0, runtime)) {
        fprintf(stderr, "error[E0902]: cannot find the runtime library libastra_rt.a "
                        "(set ASTRA_RUNTIME)\n");
        return STOP_FAIL;
    }
    const char *clang = getenv("ASTRA_CLANG");
    // The module has no target triple: clang uses its default target.
    char *argv[] = {(char *)(clang && clang[0] ? clang : "clang"), opts->release ? "-O2" : "-O0",
                    "-Wno-override-module", (char *)ll, runtime, "-o", (char *)exe, NULL};
    if (spawn_wait(argv) != 0) {
        fprintf(stderr, "error[E0902]: clang failed to compile the generated code\n");
        return STOP_FAIL;
    }
    return STOP;
}

static Next generate(Pipeline *p, Sema *sema, const AirProgram *air) {
    const Options *opts = p->opts;
    Timer *timer = p->timer;
    char *exe = p->exe;
    char ll[PATH_SIZE + 3];
    if (!output_path(opts, exe) || snprintf(ll, sizeof ll, "%s.ll", exe) >= (int)sizeof ll) {
        fprintf(stderr, "error[E0902]: cannot create the output path\n");
        return STOP_FAIL;
    }
    FILE *out = fopen(ll, "w");
    if (!out) {
        fprintf(stderr, "error[E0902]: cannot write `%s`: %s\n", ll, strerror(errno));
        return STOP_FAIL;
    }
    timer_start(timer);
    llvm_generate(sema, air, out);
    fclose(out);
    timer_report(timer, "llvm", ll);
    if (opts->emit == EMIT_LLVM) {
        exe[0] = '\0';
        return STOP;
    }
    timer_start(timer);
    Next next = link_program(opts, ll, exe);
    timer_report(timer, "clang", exe);
    remove(ll);
    if (next != STOP) {
        exe[0] = '\0';
    }
    return next;
}

Next run_backend(Pipeline *p, Sema *sema) {
    timer_start(p->timer);
    AirProgram air = {0};
    lower_module(sema, &air);
    char error[256];
    if (!air_validate(&air, error, sizeof error)) {
        fatal("invalid AIR: %s", error);
    }
    timer_report(p->timer, "lower", "");
    Next next = STOP;
    if (diags_has_errors(&sema->session->diags)) {
        next = STOP;
    } else if (p->opts->emit == EMIT_AIR) {
        air_print(&air, sema, stdout);
    } else {
        next = generate(p, sema, &air);
    }
    air_program_free(&air);
    return next;
}
