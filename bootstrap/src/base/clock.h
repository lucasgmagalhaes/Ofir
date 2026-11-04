// Monotonic clock for `--time-passes`.

#ifndef OFIR_CLOCK_H
#define OFIR_CLOCK_H

#include <time.h>

static inline double clock_now_ms(void) {
    struct timespec ts;
#ifdef _WIN32
    timespec_get(&ts, TIME_UTC);   // MinGW has clock_gettime only in winpthreads
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

#endif
