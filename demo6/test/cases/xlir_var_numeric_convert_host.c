/* Reuse the existing language regression through the XLIR backend. */
#include "../../build/test/cgen_var_numeric_convert_xlir_generated.c"

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}
