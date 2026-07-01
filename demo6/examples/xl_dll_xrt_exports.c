#include "lib/xrt/xrt.h"

#include <stdio.h>

int main(void)
{
    xvalue value;
    long long got;

    if ( xrtInit() == NULL ) {
        fprintf(stderr, "xrtInit failed\n");
        return 1;
    }
    value = xvoCreateInt(42);
    if ( value == NULL ) {
        fprintf(stderr, "xvoCreateInt failed\n");
        xrtUnit();
        return 1;
    }
    got = (long long)xvoGetInt(value);
    xvoUnref(value);
    xrtUnit();
    if ( got != 42 ) {
        fprintf(stderr, "xvoGetInt returned %lld\n", got);
        return 1;
    }
    printf("xl.dll XRT exports ok\n");
    return 0;
}
