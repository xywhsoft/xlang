#include "../../build/test/xlir_xvalue_direct_call_generated.c"

extern bool xrtValueGetInt(const xvalue* value, int64_t* result);
extern void xrtValueRelease(xvalue* value);

int main(void)
{
    int64_t result = 0;
    xvalue* value;
    xvalue* count;
    if ( xlir_module_init() != 1 ) return 1;
    value = xlir_fn_2();
    count = xrtValueObjectGet(value, xrtStrView("count"));
    if ( !xrtValueGetInt(count, &result) || result != 42 ) {
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    return 0;
}
