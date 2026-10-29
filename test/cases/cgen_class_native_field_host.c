#include <stdlib.h>

typedef struct NativeBox {
    long long value;
} NativeBox;

static long long gCloseCount = 0;
static long long gCloseValues[8];

void* native_box_open(long long value)
{
    NativeBox* box = (NativeBox*)malloc(sizeof(NativeBox));
    if ( box == 0 ) {
        exit(2);
    }
    box->value = value;
    return box;
}

void native_box_close(void* self)
{
    NativeBox* box = (NativeBox*)self;
    if ( box == 0 || gCloseCount >= 8 ) {
        exit(3);
    }
    gCloseValues[gCloseCount] = box->value;
    gCloseCount += 1;
    free(box);
}

long long native_box_value(void* self)
{
    NativeBox* box = (NativeBox*)self;
    if ( box == 0 ) {
        exit(5);
    }
    return box->value;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}

long long close_count(void)
{
    return gCloseCount;
}

long long close_value(long long index)
{
    if ( index < 0 || index >= gCloseCount ) {
        exit(4);
    }
    return gCloseValues[index];
}
