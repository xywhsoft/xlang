/* The quality gate selects one generated translation unit and verifies its
 * typed default-initializer ABI through the real module initializer. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(XLIR_VALUE_INITIALIZER_CLASS_DEFAULT)
#include "../../build/test/xlir_value_initializer_class_default_generated.c"
#elif defined(XLIR_VALUE_INITIALIZER_NATIVE_OWNERSHIP)
#include "../../build/test/xlir_value_initializer_native_ownership_generated.c"
#elif defined(XLIR_VALUE_INITIALIZER_NATIVE_NULL)
#include "../../build/test/xlir_value_initializer_native_null_generated.c"
#else
#include "../../build/test/xlir_value_initializer_default_generated.c"
#endif

typedef struct XLIR_ValueInitializerBox {
    int64_t value;
} XLIR_ValueInitializerBox;

static int64_t xlirValueInitializerCloseCount = 0;

void* make_borrowed_box(int64_t value)
{
    XLIR_ValueInitializerBox* box =
        (XLIR_ValueInitializerBox*)malloc(sizeof(*box));
    if ( box == NULL ) exit(2);
    box->value = value;
    return box;
}

void borrowed_box_close(void* handle)
{
    if ( handle == NULL ) exit(3);
    free(handle);
    xlirValueInitializerCloseCount += 1;
}

int64_t borrowed_box_value(void* handle)
{
    const XLIR_ValueInitializerBox* box =
        (const XLIR_ValueInitializerBox*)handle;
    if ( box == NULL ) exit(4);
    return box->value;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "xlir_value_initializer: actual=%lld expected=%lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

int64_t borrowed_close_count(void)
{
    return xlirValueInitializerCloseCount;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
