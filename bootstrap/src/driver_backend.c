// The back end: lowering, LLVM IR, clang (docs/design/toolchain.md, 4).

#define _XOPEN_SOURCE 700   // realpath
#define _DARWIN_C_SOURCE    // mkdtemp on macOS

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

// Each lowered function is validated, then printed (--emit=air) or turned
// into LLVM IR right away.
typedef struct {
    Sema *sema;
    Gen *gen;     // NULL for --emit=air
} Sink;

static void take_body(void *context, const AirBody *body) {
    Sink *sink = context;
    char error[256];
    if (!air_validate_body(body, error, sizeof error)) {
        air_print_body(body, sink->sema, stderr);   // what the bug report needs
        fatal("invalid AIR: %s", error);
    }
    if (sink->gen) {
        llvm_function(sink->gen, body);
    } else {
        air_print_body(body, sink->sema, stdout);
    }
}

// Lowers and writes `<exe>.ll`; returns false if lowering reported errors.
static bool write_llvm(Pipeline *p, Sema *sema, const char *ll) {
    FILE *out = fopen(ll, "w");
    if (!out) {
        fprintf(stderr, "error[E0902]: cannot write `%s`: %s\n", ll, strerror(errno));
        return false;
    }
    timer_start(p->timer);
    Sink sink = {.sema = sema, .gen = llvm_begin(sema, out)};
    lower_module(sema, take_body, &sink);
    llvm_end(sink.gen);
    fclose(out);
    timer_report(p->timer, "llvm", ll);
    return !diags_has_errors(&sema->session->diags);
}

static Next generate(Pipeline *p, Sema *sema) {
    char *exe = p->exe;
    char ll[PATH_SIZE + 3];
    if (!output_path(p->opts, exe) || snprintf(ll, sizeof ll, "%s.ll", exe) >= (int)sizeof ll) {
        fprintf(stderr, "error[E0902]: cannot create the output path\n");
        return STOP_FAIL;
    }
    bool ok = write_llvm(p, sema, ll);
    Next next = ok ? STOP : STOP_FAIL;
    if (ok && p->opts->emit != EMIT_LLVM) {
        timer_start(p->timer);
        next = link_program(p->opts, ll, exe);
        timer_report(p->timer, "clang", exe);
    }
    if (!ok || p->opts->emit != EMIT_LLVM) {
        remove(ll);
    }
    if (next != STOP || p->opts->emit == EMIT_LLVM) {
        exe[0] = '\0';
    }
    return next;
}

Next run_backend(Pipeline *p, Sema *sema) {
    if (p->opts->emit == EMIT_AIR) {
        Sink sink = {.sema = sema};
        lower_module(sema, take_body, &sink);
        return STOP;
    }
    return generate(p, sema);
}
