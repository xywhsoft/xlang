#include <stdint.h>
#include <stdlib.h>

typedef struct XlirNominalCounter {
    int64_t value;
} XlirNominalCounter;

static int64_t xlirNominalCounterClosed = 0;

void* counter_open(int64_t seed)
{
    XlirNominalCounter* counter = (XlirNominalCounter*)malloc(sizeof(*counter));
    if ( counter == NULL ) exit(2);
    counter->value = seed;
    return counter;
}

void counter_close(void* self)
{
    free(self);
    xlirNominalCounterClosed += 1;
}

int64_t counter_add(void* self, int64_t value)
{
    XlirNominalCounter* counter = (XlirNominalCounter*)self;
    counter->value += value;
    return counter->value;
}

int64_t counter_value(void* self)
{
    return ((XlirNominalCounter*)self)->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) exit(1);
}

int64_t counter_closed(void)
{
    return xlirNominalCounterClosed;
}

#include "../../build/test/xlir_native_nominal_receiver_generated.c"

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
