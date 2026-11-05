// The process (std.os): arguments, environment, exit and running programs.

#include "ofir_rt.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

static int arg_count;
static char **arg_values;

void ofir_set_args(int argc, char **argv) {
    arg_count = argc;
    arg_values = argv;
}

int64_t ofir_arg_count(void) {
    return arg_count;
}

const uint8_t *ofir_arg(int64_t i, int64_t *len) {
    *len = (int64_t)strlen(arg_values[i]);
    return (const uint8_t *)arg_values[i];
}

// The target the program was compiled for, as `@cfg` names it (spec 12.4).
const uint8_t *ofir_os_name(int64_t *len) {
#if defined(__APPLE__)
    const char *name = "macos";
#elif defined(_WIN32)
    const char *name = "windows";
#elif defined(__linux__)
    const char *name = "linux";
#else
    const char *name = "unknown";
#endif
    *len = (int64_t)strlen(name);
    return (const uint8_t *)name;
}

const uint8_t *ofir_arch_name(int64_t *len) {
#if defined(__x86_64__) || defined(_M_X64)
    const char *name = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    const char *name = "aarch64";
#else
    const char *name = "unknown";
#endif
    *len = (int64_t)strlen(name);
    return (const uint8_t *)name;
}

// A NUL-terminated copy of `len` bytes, or NULL if they contain a NUL.
char *ofir_c_string(const uint8_t *ptr, int64_t len) {
    if (memchr(ptr, 0, (size_t)len)) {
        return NULL;
    }
    char *s = ofir_alloc(len + 1);
    memcpy(s, ptr, (size_t)len);
    s[len] = 0;
    return s;
}

const uint8_t *ofir_env(const uint8_t *name, int64_t len, int64_t *value_len) {
    char *key = ofir_c_string(name, len);
    const char *value = key ? getenv(key) : NULL;
    ofir_free(key);
    *value_len = value ? (int64_t)strlen(value) : 0;
    return (const uint8_t *)value;
}

_Noreturn void ofir_exit(int64_t code) {
    ofir_stdout_flush();
    exit((int)code);
}

static void buf_push(OfirBuf *buf, int64_t *cap, const uint8_t *data, int64_t n) {
    if (buf->len + n > *cap) {
        *cap = (buf->len + n) * 2;
        buf->ptr = ofir_realloc(buf->ptr, *cap);
    }
    memcpy(buf->ptr + buf->len, data, (size_t)n);
    buf->len += n;
}

#ifdef _WIN32

// Appends `arg` to `line` (of capacity `*cap`), quoted as the C runtime of
// the child splits its command line.
static void quote_arg(char **line, int64_t *len, int64_t *cap, const char *arg) {
    int64_t need = *len + 3 + 2 * (int64_t)strlen(arg);
    if (need > *cap) {
        *cap = need * 2;
        *line = ofir_realloc(*line, *cap);
    }
    char *out = *line;
    int64_t at = *len;
    bool plain = arg[0] != '\0' && strpbrk(arg, " \t\n\v\"") == NULL;
    if (at > 0) {
        out[at++] = ' ';
    }
    if (!plain) {
        out[at++] = '"';
    }
    for (const char *c = arg;; c++) {
        int64_t slashes = 0;
        for (; *c == '\\'; c++) {
            slashes++;
        }
        bool end = *c == '\0';
        // Backslashes are doubled before a quote, including the closing one.
        int64_t copies = plain ? slashes : end ? slashes * 2 : *c == '"' ? slashes * 2 + 1
                                                                           : slashes;
        for (int64_t i = 0; i < copies; i++) {
            out[at++] = '\\';
        }
        if (end) {
            break;
        }
        out[at++] = *c;
    }
    if (!plain) {
        out[at++] = '"';
    }
    out[at] = '\0';
    *len = at;
}

typedef struct {
    HANDLE pipe;
    OfirBuf *buf;
} Reader;

// Reads a pipe until it closes (one thread per pipe, so that the child
// never waits for the other one to be drained).
static DWORD WINAPI read_pipe(void *arg) {
    Reader *reader = arg;
    int64_t cap = 0;
    uint8_t chunk[4096];
    DWORD n;
    while (ReadFile(reader->pipe, chunk, sizeof chunk, &n, NULL) && n > 0) {
        buf_push(reader->buf, &cap, chunk, n);
    }
    return 0;
}

static int32_t spawn(char **argv, OfirRun *run) {
    int64_t len = 0, cap = 256;
    char *line = ofir_alloc(cap);
    line[0] = '\0';
    for (char **arg = argv; *arg; arg++) {
        quote_arg(&line, &len, &cap, *arg);
    }
    SECURITY_ATTRIBUTES inherit = {.nLength = sizeof inherit, .bInheritHandle = TRUE};
    HANDLE out_read, out_write, err_read, err_write;
    if (!CreatePipe(&out_read, &out_write, &inherit, 0)) {
        ofir_free(line);
        return EIO;
    }
    if (!CreatePipe(&err_read, &err_write, &inherit, 0)) {
        CloseHandle(out_read);
        CloseHandle(out_write);
        ofir_free(line);
        return EIO;
    }
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA startup = {.cb = sizeof startup, .dwFlags = STARTF_USESTDHANDLES};
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = out_write;
    startup.hStdError = err_write;
    PROCESS_INFORMATION process;
    ofir_stdout_flush();
    BOOL started = CreateProcessA(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup,
                                  &process);
    DWORD error = GetLastError();
    ofir_free(line);
    CloseHandle(out_write);
    CloseHandle(err_write);
    if (!started) {
        CloseHandle(out_read);
        CloseHandle(err_read);
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ENOENT
               : error == ERROR_ACCESS_DENIED                                ? EACCES
                                                                             : EIO;
    }
    Reader err_reader = {err_read, &run->err};
    HANDLE thread = CreateThread(NULL, 0, read_pipe, &err_reader, 0, NULL);
    Reader out_reader = {out_read, &run->out};
    read_pipe(&out_reader);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    } else {
        read_pipe(&err_reader);
    }
    CloseHandle(out_read);
    CloseHandle(err_read);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 0;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    run->status = (int64_t)status;
    return 0;
}

#else

// Reads both pipes until they close, without blocking on one while the
// child waits for the other to be drained.
static void drain(int out_fd, int err_fd, OfirRun *run) {
    struct pollfd fds[2] = {{.fd = out_fd, .events = POLLIN}, {.fd = err_fd, .events = POLLIN}};
    OfirBuf *bufs[2] = {&run->out, &run->err};
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

static int32_t spawn(char **argv, OfirRun *run) {
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
    ofir_stdout_flush();
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

#endif

// `args` holds `count` strings, each followed by a NUL: the program first.
int32_t ofir_run(const uint8_t *args, int64_t len, int64_t count, OfirRun *run) {
    *run = (OfirRun){0};
    char *copy = ofir_alloc(len + 1);
    memcpy(copy, args, (size_t)len);
    copy[len] = 0;
    char **argv = ofir_alloc((count + 1) * (int64_t)sizeof(char *));
    char *at = copy;
    for (int64_t i = 0; i < count; i++) {
        argv[i] = at;
        at += strlen(at) + 1;
    }
    argv[count] = NULL;
    int32_t code = spawn(argv, run);
    ofir_free(argv);
    ofir_free(copy);
    return code;
}

// `ofir test` passes the index of the test to run as the only argument.
int64_t ofir_test_index(void) {
    return arg_count > 1 ? strtoll(arg_values[1], NULL, 10) : -1;
}
