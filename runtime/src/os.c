// The process (std.os): arguments, environment, exit and running programs.

#include "astra_rt.h"

#include <errno.h>
#include <poll.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int arg_count;
static char **arg_values;

void astra_set_args(int argc, char **argv) {
    arg_count = argc;
    arg_values = argv;
}

int64_t astra_arg_count(void) {
    return arg_count;
}

const uint8_t *astra_arg(int64_t i, int64_t *len) {
    *len = (int64_t)strlen(arg_values[i]);
    return (const uint8_t *)arg_values[i];
}

// A NUL-terminated copy of `len` bytes, or NULL if they contain a NUL.
char *astra_c_string(const uint8_t *ptr, int64_t len) {
    if (memchr(ptr, 0, (size_t)len)) {
        return NULL;
    }
    char *s = astra_alloc(len + 1);
    memcpy(s, ptr, (size_t)len);
    s[len] = 0;
    return s;
}

const uint8_t *astra_env(const uint8_t *name, int64_t len, int64_t *value_len) {
    char *key = astra_c_string(name, len);
    const char *value = key ? getenv(key) : NULL;
    astra_free(key);
    *value_len = value ? (int64_t)strlen(value) : 0;
    return (const uint8_t *)value;
}

_Noreturn void astra_exit(int64_t code) {
    astra_stdout_flush();
    exit((int)code);
}

static void buf_push(AstraBuf *buf, int64_t *cap, const uint8_t *data, int64_t n) {
    if (buf->len + n > *cap) {
        *cap = (buf->len + n) * 2;
        buf->ptr = astra_realloc(buf->ptr, *cap);
    }
    memcpy(buf->ptr + buf->len, data, (size_t)n);
    buf->len += n;
}

// Reads both pipes until they close, without blocking on one while the
// child waits for the other to be drained.
static void drain(int out_fd, int err_fd, AstraRun *run) {
    struct pollfd fds[2] = {{.fd = out_fd, .events = POLLIN}, {.fd = err_fd, .events = POLLIN}};
    AstraBuf *bufs[2] = {&run->out, &run->err};
    int64_t caps[2] = {0, 0};
    int open = 2;
    uint8_t chunk[4096];
    while (open > 0) {
        if (poll(fds, 2, -1) < 0 && errno != EINTR) {
            return;
        }
        for (int i = 0; i < 2; i++) {
            if (fds[i].fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }
            ssize_t n = read(fds[i].fd, chunk, sizeof chunk);
            if (n > 0) {
                buf_push(bufs[i], &caps[i], chunk, n);
            } else if (n == 0 || errno != EINTR) {
                close(fds[i].fd);
                fds[i].fd = -1;
                open--;
            }
        }
    }
}

static int32_t spawn(char **argv, AstraRun *run) {
    int out[2], err[2];
    if (pipe(out) != 0 || pipe(err) != 0) {
        return errno;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, out[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err[1], 2);
    posix_spawn_file_actions_addclose(&actions, out[0]);
    posix_spawn_file_actions_addclose(&actions, err[0]);
    pid_t pid;
    astra_stdout_flush();
    int code = posix_spawnp(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(out[1]);
    close(err[1]);
    if (code != 0) {
        close(out[0]);
        close(err[0]);
        return code;
    }
    drain(out[0], err[0], run);
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    run->status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return 0;
}

// `args` holds `count` strings, each followed by a NUL: the program first.
int32_t astra_run(const uint8_t *args, int64_t len, int64_t count, AstraRun *run) {
    *run = (AstraRun){0};
    char *copy = astra_alloc(len + 1);
    memcpy(copy, args, (size_t)len);
    copy[len] = 0;
    char **argv = astra_alloc((count + 1) * (int64_t)sizeof(char *));
    char *at = copy;
    for (int64_t i = 0; i < count; i++) {
        argv[i] = at;
        at += strlen(at) + 1;
    }
    argv[count] = NULL;
    int32_t code = spawn(argv, run);
    astra_free(argv);
    astra_free(copy);
    return code;
}
