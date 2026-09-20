#include <stdlib.h>

typedef struct NativeBox {
    long long value;
} NativeBox;

static long long gBorrowedCloseCount = 0;
static long long gOwnedCloseCount = 0;

void* native_box_open(long long value)
{
    NativeBox* box = (NativeBox*)malloc(sizeof(NativeBox));
    if ( box == NULL ) exit(2);
    box->value = value;
    return box;
}

long long native_box_value(void* self)
{
    NativeBox* box = (NativeBox*)self;
    if ( box == NULL ) exit(3);
    return box->value;
}

void native_box_borrowed_close(void* self)
{
    if ( self == NULL ) exit(4);
    ++gBorrowedCloseCount;
    free(self);
}

void native_box_owned_close(void* self)
{
    if ( self == NULL ) exit(5);
    ++gOwnedCloseCount;
    free(self);
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) exit(1);
}

long long borrowed_close_count(void)
{
    return gBorrowedCloseCount;
}

long long owned_close_count(void)
{
    return gOwnedCloseCount;
}
