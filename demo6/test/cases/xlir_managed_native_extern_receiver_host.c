#include <stdint.h>
#include <stdlib.h>

typedef struct XlirBorrowedProbe {
    int64_t value;
} XlirBorrowedProbe;

static XlirBorrowedProbe xlirBorrowedProbes[] = {
    {0}, {41}, {82}
};

void* borrowed_probe_at(int64_t index)
{
    if ( index < 1 || index > 2 ) exit(2);
    return &xlirBorrowedProbes[index];
}

int64_t borrowed_probe_value(void* self)
{
    if ( self == NULL ) exit(3);
    return ((XlirBorrowedProbe*)self)->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) exit(1);
}
