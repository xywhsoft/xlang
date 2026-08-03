#include "lib/xrt/xrt.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    xvalue value;
    xvalue copied;
    xvalue output;
    int64_t number;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    value = xl6_class_make(41);
    if ( value == NULL || xvoIsNull(value) ) {
        xlUnit();
        return 2;
    }
    copied = xvoCopy(value);
    if ( copied != NULL ) {
        xvoUnref(copied);
        xvoUnref(value);
        xlUnit();
        return 5;
    }
    xvoAddRef(value);
    copied = value;
    if ( xl6_class_read(copied) != 42 ) {
        xvoUnref(copied);
        xvoUnref(value);
        xlUnit();
        return 6;
    }
    xvoUnref(copied);
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
        xlUnit();
        return 3;
    }
    xvoUnref(value);
    xvoUnref(output);
    xlUnit();
    return result == 42 ? 0 : 4;
}
