#include "../../build/test/xlir_xvalue_basic_generated.c"

int main(void)
{
    xvalue* value;
    if (xlir_module_init() != 1 || xlir_fn_2() != 29) return 1;
    value = xlir_fn_1();
    if (value == NULL) return 1;
    xrtValueRelease(value);
    return 0;
}
