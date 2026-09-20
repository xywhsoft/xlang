#include "../../build/test/xlir_xvalue_object_string_generated.c"

#include <string.h>

typedef struct xrt_string_view {
    const char* Data;
    size_t Size;
} xrt_string_view;

extern bool xrtValueGetString(const xvalue* value, xrt_string_view* result);
extern void xrtValueRelease(xvalue* value);

int main(void)
{
    xrt_string_view result = { 0 };
    xvalue* value;
    if ( xlir_module_init() != 1 ) return 1;
    value = xlir_fn_1();
    if ( !xrtValueGetString(value, &result) || result.Size != 5 ||
         memcmp(result.Data, "hello", 5) != 0 ) {
        xrtValueRelease(value);
        return 1;
    }
    xrtValueRelease(value);
    return 0;
}
