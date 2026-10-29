#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "expect_i64 failed: actual=%lld expected=%lld\n", actual, expected);
        exit(1);
    }
}

void expect_bool(int actual, int expected)
{
    if ( (actual != 0) != (expected != 0) ) {
        fprintf(stderr, "expect_bool failed: actual=%d expected=%d\n", actual, expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "expect_str failed: actual=%s expected=%s\n",
            actual ? actual : "(null)",
            expected ? expected : "(null)");
        exit(1);
    }
}

long long native_add_i64(long long left, long long right)
{
    return left + right;
}

int native_flip_bool(int value)
{
    return value == 0;
}

long long host_counter = 40;

static long long g_destroy_count = 0;
static long long g_destroy_value = 0;

void destroy_reset(void)
{
    g_destroy_count = 0;
    g_destroy_value = 0;
}

void record_destroy(long long value)
{
    g_destroy_count += 1;
    g_destroy_value = value;
}

long long destroy_count(void)
{
    return g_destroy_count;
}

long long destroy_value(void)
{
    return g_destroy_value;
}

typedef struct Counter {
    long long value;
} Counter;

static long long g_counter_closed = 0;

void counter_reset(void)
{
    g_counter_closed = 0;
}

void* counter_open(long long seed)
{
    Counter* counter = (Counter*)malloc(sizeof(Counter));
    if ( counter == NULL ) {
        exit(2);
    }
    counter->value = seed;
    return counter;
}

long long counter_close(void* self)
{
    if ( self != NULL ) {
        free(self);
    }
    g_counter_closed += 1;
    return 0;
}

long long counter_add(void* self, long long value)
{
    Counter* counter = (Counter*)self;
    counter->value += value;
    return counter->value;
}

long long counter_value(void* self)
{
    Counter* counter = (Counter*)self;
    return counter->value;
}

long long counter_closed(void)
{
    return g_counter_closed;
}

#ifndef XL6_EXAMPLE_NO_STANDALONE_MAIN
int main(void)
{
    expect_i64(native_add_i64(20, 22), 42);
    expect_bool(native_flip_bool(0), 1);
    expect_str("xlang", "xlang");
    printf("feature_suite host ok\n");
    return 0;
}
#endif
