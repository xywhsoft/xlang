/* probe_c.c - C companion for probe.xl edge-case tests.
 *
 * NOTE: function/variable names MUST match the `extern "name"` strings used on
 * the xlang side, because that string is the exact C symbol the linker needs.
 */
#include <stdlib.h>

long long probe_rw = 11;

long long probe_get_rw(void)
{
    return probe_rw;
}

void probe_set_rw(long long v)
{
    probe_rw = v;
}

/* read xlang-exported variable probe_xhits */
extern long long probe_xhits;
long long probe_read_xhits(void)
{
    return probe_xhits;
}

/* call xlang-exported function probe_xadd */
extern long long probe_xadd(long long a, long long b);
long long probe_read_xlang_sum(long long a, long long b)
{
    return probe_xadd(a, b);
}

/* mixed native-backed class handle */
typedef struct {
    long long value;
} probe_mcounter_s;

void* probe_mcounter_open(long long seed)
{
    probe_mcounter_s* c = (probe_mcounter_s*)malloc(sizeof(probe_mcounter_s));
    if (c) c->value = seed;
    return c;
}

void probe_mcounter_close(void* h)
{
    if (h) free(h);
}

long long probe_mcounter_add(void* h, long long v)
{
    probe_mcounter_s* c = (probe_mcounter_s*)h;
    if (!c) return 0;
    c->value += v;
    return c->value;
}
