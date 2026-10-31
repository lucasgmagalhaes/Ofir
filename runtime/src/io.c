// Standard output buffer (docs/design/runtime.md, 2): line buffered on a
// terminal, block buffered (64 KiB) otherwise.

#include "astra_rt.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE (64 * 1024)

static uint8_t buffer[BUFFER_SIZE];
static int64_t used;
static int line_buffered = -1;   // unknown until the first write

void astra_write(int fd, const uint8_t *ptr, int64_t len) {
    while (len > 0) {
        ssize_t n = write(fd, ptr, (size_t)len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;   // nowhere to report the error: a closed pipe, a full disk
        }
        ptr += n;
        len -= n;
    }
}

void astra_stdout_flush(void) {
    astra_write(1, buffer, used);
    used = 0;
}

void astra_stdout_write(const uint8_t *ptr, int64_t len) {
    if (line_buffered < 0) {
        line_buffered = isatty(1);
    }
    if (used + len > BUFFER_SIZE) {
        astra_stdout_flush();
        if (len > BUFFER_SIZE) {
            astra_write(1, ptr, len);
            return;
        }
    }
    memcpy(buffer + used, ptr, (size_t)len);
    used += len;
    if (line_buffered && memchr(ptr, '\n', (size_t)len)) {
        astra_stdout_flush();
    }
}

void astra_print_str(const uint8_t *ptr, int64_t len) {
    astra_stdout_write(ptr, len);
}

void astra_print_int(int64_t value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_stdout_write((const uint8_t *)text, astra_format_int(text, value));
}

void astra_print_uint(uint64_t value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_stdout_write((const uint8_t *)text, astra_format_uint(text, value));
}

void astra_print_float(double value) {
    char text[ASTRA_FORMAT_SIZE];
    astra_stdout_write((const uint8_t *)text, astra_format_float(text, value));
}

void astra_print_bool(bool value) {
    astra_stdout_write((const uint8_t *)(value ? "true" : "false"), value ? 4 : 5);
}

void astra_print_newline(void) {
    astra_stdout_write((const uint8_t *)"\n", 1);
}
