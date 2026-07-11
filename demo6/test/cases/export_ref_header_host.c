#include "lib/xrt/xrt.h"

int main(void)
{
    xvalue value;
    int result;

    if ( xrtInit() == NULL ) {
        return 1;
    }
    value = xl6_ref_make(41);
    if ( value == NULL || xvoIsNull(value) ) {
        xrtUnit();
        return 2;
    }
    result = (int)xl6_ref_read(value);
    xvoUnref(value);
    xrtUnit();
    return result == 42 ? 0 : 3;
}
