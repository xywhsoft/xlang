#include "../../build/test/xlir_closure_xvalue_capture_generated.c"

extern bool xrtValueGetInt(const xvalue* value, int64_t* result);
extern void xrtValueRelease(xvalue* value);

int main(void)
{
    int64_t result = 0;
    xvalue* value;
    xvalue* count;
    xlir_result_xvalue returned;
    if ( xlir_module_init() != 1 ) return 1;
    returned = xlir_fn_3();
    if ( !returned.ok || returned.value == NULL ) return 1;
    value = returned.value;
    count = xrtValueObjectGet(value, xrtStrView("count"));
    if ( !xrtValueGetInt(count, &result) || result != 42 ) {
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    return 0;
}
