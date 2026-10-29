#include <stdlib.h>

typedef struct NativeSizedValue {
    long long count;
} NativeSizedValue;

void* native_sized_open(long long value)
{
    NativeSizedValue* result =
        (NativeSizedValue*)malloc(sizeof(NativeSizedValue));
    if ( result == NULL ) exit(2);
    result->count = value;
    return result;
}

void native_sized_close(void* self)
{
    free(self);
}

long long native_sized_count(void* self)
{
    NativeSizedValue* value = (NativeSizedValue*)self;
    if ( value == NULL ) exit(3);
    return value->count;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) exit(1);
}
