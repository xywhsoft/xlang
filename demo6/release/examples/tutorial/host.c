#include <stdlib.h>

long long host_counter = 40;

long long host_add(long long left, long long right)
{
    return left + right;
}

int host_flip(int value)
{
    return value == 0;
}

static long long g_destroy_count = 0;
static long long g_destroy_value = 0;

void reset_destroy_log(void)
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

typedef struct TutorialCounter {
    long long value;
} TutorialCounter;

static long long g_counter_closed = 0;

void counter_reset(void)
{
    g_counter_closed = 0;
}

void* counter_open(long long seed)
{
    TutorialCounter* counter = (TutorialCounter*)malloc(sizeof(TutorialCounter));
    if ( counter == 0 ) {
        exit(2);
    }
    counter->value = seed;
    return counter;
}

long long counter_close(void* self)
{
    if ( self != 0 ) {
        free(self);
    }
    g_counter_closed += 1;
    return 0;
}

long long counter_add(void* self, long long value)
{
    TutorialCounter* counter = (TutorialCounter*)self;
    counter->value += value;
    return counter->value;
}

long long counter_value(void* self)
{
    TutorialCounter* counter = (TutorialCounter*)self;
    return counter->value;
}

long long counter_closed(void)
{
    return g_counter_closed;
}

#ifndef XL6_EXAMPLE_NO_STANDALONE_MAIN
#include <stdio.h>

int main(void)
{
    if ( host_add(20, 22) != 42 ) {
        return 1;
    }
    if ( host_flip(0) != 1 ) {
        return 2;
    }
    printf("tutorial host ok\n");
    return 0;
}
#endif
