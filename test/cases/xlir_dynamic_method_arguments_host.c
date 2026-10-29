#include "../../build/test/cgen_dynamic_method_arguments_xlir_generated.c"

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}
