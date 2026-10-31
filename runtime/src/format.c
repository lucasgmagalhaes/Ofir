// Number formatting for printing and interpolation.

#include "astra_rt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int astra_format_uint(char *buf, uint64_t value) {
    char digits[24];
    int n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    for (int i = 0; i < n; i++) {
        buf[i] = digits[n - 1 - i];
    }
    return n;
}

int astra_format_int(char *buf, int64_t value) {
    if (value >= 0) {
        return astra_format_uint(buf, (uint64_t)value);
    }
    buf[0] = '-';
    return 1 + astra_format_uint(buf + 1, 0 - (uint64_t)value);   // also for INT64_MIN
}

// Removes the `+` and the leading zeros of an exponent: `1e+07` -> `1e7`.
static int tidy_exponent(char *buf, int n) {
    char *e = strchr(buf, 'e');
    if (!e) {
        return n;
    }
    char *digits = e + 1;
    char *out = digits;
    if (*digits == '-') {
        out++;
        digits++;
    } else if (*digits == '+') {
        digits++;
    }
    while (*digits == '0' && digits[1] != '\0') {
        digits++;
    }
    memmove(out, digits, strlen(digits) + 1);
    return (int)strlen(buf);
}

// The shortest decimal form that reads back as the same value (spec 2.1):
// fixed notation for magnitudes in [1e-5, 1e16), an exponent otherwise, and
// always a fraction or an exponent, so that `1.0` is not printed as `1`.
static int format_float(char *buf, double value, bool is_f32) {
    if (isnan(value)) {
        memcpy(buf, "NaN", 4);
        return 3;
    }
    if (isinf(value)) {
        strcpy(buf, value > 0 ? "inf" : "-inf");
        return value > 0 ? 3 : 4;
    }
    double magnitude = fabs(value);
    bool fixed = magnitude == 0 || (magnitude >= 1e-5 && magnitude < 1e16);
    int n = 0;
    for (int precision = fixed ? 0 : 1; precision <= (fixed ? 22 : 17); precision++) {
        int digits = fixed ? precision : precision - 1;
        n = snprintf(buf, ASTRA_FORMAT_SIZE, fixed ? "%.*f" : "%.*e", digits, value);
        bool exact = is_f32 ? strtof(buf, NULL) == (float)value : strtod(buf, NULL) == value;
        if (exact) {
            break;
        }
    }
    n = tidy_exponent(buf, n);
    if (!strpbrk(buf, ".e")) {
        memcpy(buf + n, ".0", 3);
        n += 2;
    }
    return n;
}

int astra_format_float(char *buf, double value) {
    return format_float(buf, value, false);
}

int astra_format_f32(char *buf, float value) {
    return format_float(buf, value, true);
}
