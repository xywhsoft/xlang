#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

typedef struct BorrowedBox {
    int64_t value;
} BorrowedBox;

static int64_t gCloseCount = 0;

void* make_borrowed_box(int64_t value)
{
    BorrowedBox* box = (BorrowedBox*)malloc(sizeof(BorrowedBox));
    if ( box == 0 ) {
        exit(2);
    }
    box->value = value;
    return box;
}

void borrowed_box_close(void* self)
{
    BorrowedBox* box = (BorrowedBox*)self;
    if ( box == 0 ) {
        exit(3);
    }
    free(box);
    gCloseCount += 1;
}

int64_t borrowed_box_value(void* self)
{
    BorrowedBox* box = (BorrowedBox*)self;
    if ( box == 0 ) {
        exit(4);
    }
    return box->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "cgen_native_class_bind_point: actual=%" PRId64 " expected=%" PRId64 "\n", actual, expected);
        exit(1);
    }
}

int64_t borrowed_close_count(void)
{
    return gCloseCount;
}
