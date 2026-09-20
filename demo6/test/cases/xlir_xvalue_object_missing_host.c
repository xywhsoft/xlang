#include "../../build/test/xlir_xvalue_object_missing_generated.c"

extern xvalue* xrtValueNull(void);
extern void xrtValueRelease(xvalue* value);

int main(void)
{
    xvalue* value;
    if ( xlir_module_init() != 1 ) return 1;
    value = xlir_fn_1();
    if ( value != xrtValueNull() ) {
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    return 0;
}
