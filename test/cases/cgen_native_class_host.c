#include <stdlib.h>

typedef struct Counter {
    long long value;
} Counter;

static long long gCounterClosed = 0;

void* counter_open(long long seed)
{
    Counter* p = (Counter*)malloc(sizeof(Counter));
    if ( p == 0 ) {
        exit(2);
    }
    p->value = seed;
    return p;
}

long long counter_close(void* self)
{
    free(self);
    gCounterClosed += 1;
    return 0;
}

long long counter_add(void* self, long long value)
{
    Counter* p = (Counter*)self;
    p->value += value;
    return p->value;
}

long long counter_value(void* self)
{
    Counter* p = (Counter*)self;
    return p->value;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}

long long counter_closed(void)
{
    return gCounterClosed;
}
