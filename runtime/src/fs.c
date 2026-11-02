// Files, directories and standard input (std.fs, std.io). Every function
// returns 0 or an errno value, which std turns into an IoError.

#include "ofir_rt.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static int32_t read_fd(int fd, OfirBuf *out) {
    int64_t cap = 4096;
    *out = (OfirBuf){ofir_alloc(cap), 0};
    for (;;) {
        if (out->len == cap) {
            cap *= 2;
            out->ptr = ofir_realloc(out->ptr, cap);
        }
        ssize_t n = read(fd, out->ptr + out->len, (size_t)(cap - out->len));
        if (n == 0) {
            return 0;
        }
        if (n < 0 && errno != EINTR) {
            int32_t code = errno;
            ofir_free(out->ptr);
            *out = (OfirBuf){0};
            return code;
        }
        out->len += n > 0 ? n : 0;
    }
}

int32_t ofir_read_file(const uint8_t *path, int64_t len, OfirBuf *out) {
    char *name = ofir_c_string(path, len);
    int fd = name ? open(name, O_RDONLY) : -1;
    int32_t code = name ? errno : EINVAL;
    ofir_free(name);
    if (fd < 0) {
        *out = (OfirBuf){0};
        return code;
    }
    code = read_fd(fd, out);
    close(fd);
    return code;
}

int32_t ofir_write_file(const uint8_t *path, int64_t len, const uint8_t *data, int64_t n) {
    char *name = ofir_c_string(path, len);
    int fd = name ? open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666) : -1;
    int32_t code = name ? errno : EINVAL;
    ofir_free(name);
    if (fd < 0) {
        return code;
    }
    while (n > 0) {
        ssize_t done = write(fd, data, (size_t)n);
        if (done < 0 && errno != EINTR) {
            code = errno;
            close(fd);
            return code;
        }
        data += done > 0 ? done : 0;
        n -= done > 0 ? done : 0;
    }
    return close(fd) == 0 ? 0 : errno;
}

bool ofir_path_exists(const uint8_t *path, int64_t len) {
    char *name = ofir_c_string(path, len);
    struct stat info;
    bool found = name && stat(name, &info) == 0;
    ofir_free(name);
    return found;
}

int32_t ofir_canonicalize(const uint8_t *path, int64_t len, OfirBuf *out) {
    char *name = ofir_c_string(path, len);
    char full[PATH_MAX];
    *out = (OfirBuf){0};
    if (!name) {
        return EINVAL;
    }
    bool found = realpath(name, full) != NULL;
    ofir_free(name);
    if (!found) {
        return errno;
    }
    out->len = (int64_t)strlen(full);
    out->ptr = ofir_alloc(out->len);
    memcpy(out->ptr, full, (size_t)out->len);
    return 0;
}

int32_t ofir_remove_file(const uint8_t *path, int64_t len) {
    char *name = ofir_c_string(path, len);
    int32_t code = !name ? EINVAL : unlink(name) == 0 ? 0 : errno;
    ofir_free(name);
    return code;
}

// Creates each missing directory of `name`, which it modifies in place.
static int32_t make_dirs(char *name) {
    for (char *at = name + 1;; at++) {
        if (*at != '/' && *at != 0) {
            continue;
        }
        char saved = *at;
        *at = 0;
        if (mkdir(name, 0777) != 0 && errno != EEXIST) {
            return errno;
        }
        *at = saved;
        if (saved == 0) {
            return 0;
        }
    }
}

int32_t ofir_create_dir_all(const uint8_t *path, int64_t len) {
    char *name = ofir_c_string(path, len);
    int32_t code = name && len > 0 ? make_dirs(name) : EINVAL;
    ofir_free(name);
    return code;
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void list_names(DIR *dir, OfirBuf *out) {
    char **names = NULL;
    size_t count = 0, cap = 0;
    for (struct dirent *entry; (entry = readdir(dir));) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            names = ofir_realloc(names, (int64_t)(cap * sizeof(char *)));
        }
        names[count++] = strdup(entry->d_name);
    }
    qsort(names, count, sizeof(char *), compare_names);   // sorted, for determinism
    int64_t size = 0;
    for (size_t i = 0; i < count; i++) {
        size += (int64_t)strlen(names[i]) + 1;
    }
    *out = (OfirBuf){ofir_alloc(size), 0};
    for (size_t i = 0; i < count; i++) {
        size_t n = strlen(names[i]) + 1;
        memcpy(out->ptr + out->len, names[i], n);
        out->len += (int64_t)n;
        free(names[i]);
    }
    ofir_free(names);
}

// The names in the directory, sorted, each followed by a NUL.
int32_t ofir_read_dir(const uint8_t *path, int64_t len, OfirBuf *out) {
    char *name = ofir_c_string(path, len);
    DIR *dir = name ? opendir(name) : NULL;
    int32_t code = name ? errno : EINVAL;
    ofir_free(name);
    *out = (OfirBuf){0};
    if (!dir) {
        return code;
    }
    list_names(dir, out);
    closedir(dir);
    return 0;
}

// One line of standard input with its `\n`; an empty buffer at the end.
int32_t ofir_read_line(OfirBuf *out) {
    ofir_stdout_flush();   // a prompt appears before the program waits
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    *out = (OfirBuf){0};
    if (n < 0) {
        free(line);
        return ferror(stdin) ? errno : 0;
    }
    out->ptr = ofir_alloc(n);
    memcpy(out->ptr, line, (size_t)n);
    out->len = n;
    free(line);
    return 0;
}

int32_t ofir_read_stdin(OfirBuf *out) {
    ofir_stdout_flush();
    return read_fd(0, out);
}

int32_t ofir_current_dir(OfirBuf *out) {
    char path[4096];
    *out = (OfirBuf){0};
    if (!getcwd(path, sizeof path)) {
        return errno;
    }
    out->len = (int64_t)strlen(path);
    out->ptr = ofir_alloc(out->len);
    memcpy(out->ptr, path, (size_t)out->len);
    return 0;
}

int32_t ofir_executable_path(OfirBuf *out) {
    char path[4096];
    *out = (OfirBuf){0};
#ifdef __APPLE__
    uint32_t size = sizeof path;
    ssize_t n = _NSGetExecutablePath(path, &size) == 0 ? (ssize_t)strlen(path) : -1;
#else
    ssize_t n = readlink("/proc/self/exe", path, sizeof path);
#endif
    if (n < 0) {
        return errno;
    }
    out->ptr = ofir_alloc(n);
    memcpy(out->ptr, path, (size_t)n);
    out->len = n;
    return 0;
}

// The IoErrorKind of an errno value: NotFound, PermissionDenied,
// AlreadyExists, InvalidInput, UnexpectedEof, Interrupted, Other.
int32_t ofir_error_kind(int32_t code) {
    switch (code) {
    case ENOENT: case ENOTDIR: return 0;
    case EACCES: case EPERM: return 1;
    case EEXIST: return 2;
    case EINVAL: return 3;
    case EINTR: return 5;
    default: return 6;
    }
}

const uint8_t *ofir_error_message(int32_t code, int64_t *len) {
    const char *text = strerror(code);
    *len = (int64_t)strlen(text);
    return (const uint8_t *)text;
}
