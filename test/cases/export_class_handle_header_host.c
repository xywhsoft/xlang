#include "../../release/include/xruntime_decl.h"
#include "../../build/test/export_class_handle.h"

int main(void)
{
    xvalue* value;
    xvalue* copied;
    xvalue* output;
    int64_t number;
    int result;

    value = xl6_class_make(41);
    if ( value == NULL ) {
        return 2;
    }
    copied = xrtValueClone(value);
    if ( copied == NULL || copied == value || xl6_class_read(copied) != 42 ) {
        xrtValueRelease(copied);
        xrtValueRelease(value);
        return 5;
    }
    xrtValueRelease(copied);
    copied = xrtValueRetain(value);
    if ( xl6_class_read(copied) != 42 ) {
        xrtValueRelease(copied);
        xrtValueRelease(value);
        return 6;
    }
    xrtValueRelease(copied);
    result = (int)xl6_class_read(value);
    number = 1;
    number = xl6_mode_increment(number);
    output = xl6_mode_make();
    if ( number != 2 || output == NULL || xl6_class_read(output) != 10 ) {
        xrtValueRelease(value);
        if ( output != NULL ) {
            xrtValueRelease(output);
        }
        return 3;
    }
    xrtValueRelease(value);
    xrtValueRelease(output);
    return result == 42 ? 0 : 4;
}
