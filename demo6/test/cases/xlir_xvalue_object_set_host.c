#include "../../build/test/xlir_xvalue_object_set_generated.c"

extern bool xrtValueGetInt(const xvalue* value, int64_t* result);
extern void xrtValueRelease(xvalue* value);

int main(void)
{
    int64_t result = 0;
    xlir_result_xvalue call;
    xvalue* value;
    if ( xlir_module_init() != 1 ) return 1;
    call = xlir_fn_1();
    if ( !call.ok ) return 1;
    value = call.value;
    if ( !xrtValueGetInt(value, &result) || result != 42 ) {
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    return 0;
}
