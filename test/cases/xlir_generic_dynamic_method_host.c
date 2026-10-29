#include <string.h>

#include "../../build/test/cgen_generic_dynamic_method_xlir_generated.c"

size_t xl6_xrt_string_char_len(const char* text)
{
    return text != NULL ? strlen(text) : 0;
}

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    return result;
}
