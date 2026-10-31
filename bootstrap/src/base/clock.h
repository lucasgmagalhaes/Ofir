// Monotonic clock for `--time-passes`.

#ifndef ASTRA_CLOCK_H
#define ASTRA_CLOCK_H

#include <time.h>

static inline double clock_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

#endif
