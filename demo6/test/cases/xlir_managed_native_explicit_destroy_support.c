#include <stdint.h>
#include <stdlib.h>

typedef struct XlirManagedCounter {
    int64_t value;
} XlirManagedCounter;

static int64_t xlirManagedCounterClosed = 0;

void* managed_counter_open(int64_t seed)
{
    XlirManagedCounter* counter =
        (XlirManagedCounter*)malloc(sizeof(*counter));
    if ( counter == NULL ) exit(2);
    counter->value = seed;
    return counter;
}

void managed_counter_close(void* self)
{
    if ( self == NULL ) exit(3);
    free(self);
    xlirManagedCounterClosed += 1;
}

int64_t managed_counter_add(void* self, int64_t value)
{
    XlirManagedCounter* counter = (XlirManagedCounter*)self;
    if ( counter == NULL ) exit(4);
    counter->value += value;
    return counter->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) exit(5);
}

int64_t managed_counter_closed(void)
{
    return xlirManagedCounterClosed;
}
