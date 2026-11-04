// Operating system services of the driver, for POSIX systems and Windows:
// paths, temporary directories and running programs.

#ifndef OFIR_PLATFORM_H
#define OFIR_PLATFORM_H

#include "base/common.h"

#define PLATFORM_PATH_SIZE 4096

// The absolute path of an existing file, with `/` separators.
bool platform_realpath(const char *path, char out[PLATFORM_PATH_SIZE]);

// The path of the running executable; `argv0` is the fallback.
bool platform_exe_path(const char *argv0, char out[PLATFORM_PATH_SIZE]);

// Creates a new directory `<temp>/<prefix>XXXXXX` and writes its path.
bool platform_make_temp_dir(const char *prefix, char out[PLATFORM_PATH_SIZE]);

// Runs `argv` (searched in PATH) and waits: its exit status, or -1 if it
// could not be started (the reason is printed).
int platform_spawn_wait(char *const argv[]);

#endif
