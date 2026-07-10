#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct xvalue_struct* xvalue;

xvalue xvoCreateInt(long long iVal);
long long xvoGetInt(xvalue pVal);

static long long g_nc_probe_count = 0;
static long long g_recorded_xvalue = 0;

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "expect_i64 failed: actual=%lld expected=%lld\n", actual, expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "expect_str failed: actual=%s expected=%s\n", actual != NULL ? actual : "(null)", expected != NULL ? expected : "(null)");
        exit(1);
    }
}

void nc_probe_reset(void)
{
    g_nc_probe_count = 0;
}

long long nc_probe_next(void)
{
    g_nc_probe_count += 1;
    return 33;
}

xvalue nc_probe_next_var(void)
{
    g_nc_probe_count += 1;
    return xvoCreateInt(33);
}

long long nc_probe_count(void)
{
    return g_nc_probe_count;
}

void record_xvalue(xvalue value)
{
    g_recorded_xvalue = xvoGetInt(value);
}

long long recorded_xvalue(void)
{
    return g_recorded_xvalue;
}

void* native_handle(long long value)
{
    static long long slots[8];
    int index = (int)(value & 7);
    slots[index] = value;
    return &slots[index];
}
