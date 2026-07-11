#include <stdlib.h>

typedef struct BorrowedBox {
    long long value;
} BorrowedBox;

static long long gCloseCount = 0;

void* make_borrowed_box(long long value)
{
    BorrowedBox* box = (BorrowedBox*)malloc(sizeof(BorrowedBox));
    if ( box == 0 ) {
        exit(2);
    }
    box->value = value;
    return box;
}

long long borrowed_box_close(void* self)
{
    BorrowedBox* box = (BorrowedBox*)self;
    if ( box == 0 ) {
        exit(3);
    }
    free(box);
    gCloseCount += 1;
    return 0;
}

long long borrowed_box_value(void* self)
{
    BorrowedBox* box = (BorrowedBox*)self;
    if ( box == 0 ) {
        exit(4);
    }
    return box->value;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}

long long borrowed_close_count(void)
{
    return gCloseCount;
}
