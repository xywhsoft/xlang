#include "xrt_decl.h"

#include <stdio.h>

int main(void)
{
    xvalue* value;
    int64 got;

    value = xrtValueInt(42);
    if ( value == NULL ) {
        fprintf(stderr, "xrtValueInt failed\n");
        return 1;
    }
    if ( !xrtValueGetInt(value, &got) ) {
        fprintf(stderr, "xrtValueGetInt failed\n");
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    if ( got != 42 ) {
        fprintf(stderr, "xrtValueGetInt returned %lld\n", (long long)got);
        return 1;
    }
    printf("xl.dll XRT exports ok\n");
    return 0;
}
