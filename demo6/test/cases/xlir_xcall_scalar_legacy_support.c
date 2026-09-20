#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct xvalue xvalue;
extern bool xrtValueGetBool(const xvalue*, bool*);
extern bool xrtValueGetFloat(const xvalue*, double*);

void expect_xvalue_bool(xvalue* actual, bool expected)
{
    bool value = false;
    if ( !xrtValueGetBool(actual, &value) || value != expected ) {
        fprintf(stderr, "legacy scalar xcall bool mismatch\n");
        exit(1);
    }
}

void expect_xvalue_float(xvalue* actual, double expected)
{
    double value = 0.0;
    if ( !xrtValueGetFloat(actual, &value) || value != expected ) {
        fprintf(stderr, "legacy scalar xcall float mismatch\n");
        exit(1);
    }
}
