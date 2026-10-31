// Range checks of `as` conversions (spec 2.1): whether a value fits the
// target integer type. The result is an i1 that is true when it does not.

#include "codegen/llvm_internal.h"

#include <inttypes.h>
#include <math.h>
#include <string.h>

static const uint32_t INT_BITS[] = {8, 16, 32, 64, 8, 16, 32, 64};

static bool is_signed(const Ty *ty) {
    return ty->data <= INT_I64;
}

// Combines two optional failure conditions with `or`.
static void either(Gen *g, const char *low, const char *high, Value *result) {
    if (low[0] && high[0]) {
        next_temp(g, result->text);
        out_fmt(&g->out, "  %s = or i1 %s, %s\n", result->text, low, high);
    } else {
        fmt_into(result->text, sizeof result->text, "%s", low[0] ? low : high[0] ? high : "false");
    }
}

// Integer to integer: compare with the target bounds that the source type
// can exceed.
static void int_fails(Gen *g, Value a, const Ty *from, const Ty *to, Value *result) {
    uint32_t s = INT_BITS[from->data], t = INT_BITS[to->data];
    char low[16] = "", high[16] = "";
    bool below = is_signed(from) && (!is_signed(to) || t < s);
    if (below) {   // the target minimum: 0 or -2^(t-1)
        next_temp(g, low);
        int64_t min = is_signed(to) ? -(int64_t)(1ull << (t - 1)) : 0;
        out_fmt(&g->out, "  %s = icmp slt %s %s, %" PRId64 "\n", low, a.type, a.text, min);
    }
    bool above = is_signed(from) ? (is_signed(to) ? t < s : t + 1 < s)
                                 : (is_signed(to) ? t <= s : t < s);
    if (above) {
        next_temp(g, high);
        uint64_t max = is_signed(to) ? (1ull << (t - 1)) - 1 : t == 64 ? UINT64_MAX
                                                                      : (1ull << t) - 1;
        out_fmt(&g->out, "  %s = icmp %s %s %s, %" PRIu64 "\n", high,
                is_signed(from) ? "sgt" : "ugt", a.type, a.text, max);
    }
    either(g, low, high, result);
}

static void float_bound(char *out, double bound) {
    uint64_t bits;
    memcpy(&bits, &bound, sizeof bits);
    fmt_into(out, 32, "0x%016" PRIX64, bits);
}

// Float to integer: the value must be in [min, max + 1). NaN fails both
// ordered comparisons. The bounds are powers of two, exact in f32 and f64.
static void float_fails(Gen *g, Value a, const Ty *to, Value *result) {
    uint32_t t = INT_BITS[to->data];
    double min = is_signed(to) ? -ldexp(1.0, (int)t - 1) : 0.0;
    double end = is_signed(to) ? ldexp(1.0, (int)t - 1) : ldexp(1.0, (int)t);
    char lo[32], hi[32], ge[16], lt[16], ok[16];
    float_bound(lo, min);
    float_bound(hi, end);
    next_temp(g, ge);
    out_fmt(&g->out, "  %s = fcmp oge %s %s, %s\n", ge, a.type, a.text, lo);
    next_temp(g, lt);
    out_fmt(&g->out, "  %s = fcmp olt %s %s, %s\n", lt, a.type, a.text, hi);
    next_temp(g, ok);
    out_fmt(&g->out, "  %s = and i1 %s, %s\n", ok, ge, lt);
    next_temp(g, result->text);
    out_fmt(&g->out, "  %s = xor i1 %s, true\n", result->text, ok);
}

void gen_cast_fails(Gen *g, const Rvalue *rv, Value *result) {
    Value a = gen_operand(g, rv->a);
    const Ty *from = type_get(g->types, rv->a.type), *to = type_get(g->types, rv->to);
    result->type = "i1";
    if (from->kind == TY_FLOAT) {
        float_fails(g, a, to, result);
    } else {
        int_fails(g, a, from, to, result);
    }
}
