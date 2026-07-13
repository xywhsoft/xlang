#include "lib/xrt/xrt.h"

int main(void)
{
    xvalue value;
    xvalue output;
    int64_t number;
    int result;

    if ( xrtInit() == NULL ) {
        return 1;
    }
    value = xl6_class_make(41);
    if ( value == NULL || xvoIsNull(value) ) {
        xrtUnit();
        return 2;
    }
    result = (int)xl6_class_read(value);
    number = 1;
    xl6_mode_increment(&number);
    output = NULL;
    xl6_mode_make(&output);
    if ( number != 2 || output == NULL || xl6_class_read(output) != 10 ) {
        xvoUnref(value);
        if ( output != NULL ) {
            xvoUnref(output);
        }
        xrtUnit();
        return 3;
    }
    xvoUnref(value);
    xvoUnref(output);
    xrtUnit();
    return result == 42 ? 0 : 4;
}
