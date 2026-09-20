/* The product-result gate generates the included translation unit before
 * compiling this host.  Including it here lets -Werror validate the exact
 * named-struct return ABI at both declaration and call sites. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(XLIR_PRODUCT_CLASS_METHOD)
#include "../../build/test/xlir_product_result_class_generated.c"
#elif defined(XLIR_PRODUCT_CLOSURE)
#include "../../build/test/xlir_product_result_closure_generated.c"
#elif defined(XLIR_PRODUCT_GENERIC)
#include "../../build/test/xlir_product_result_generic_generated.c"
#else
#include "../../build/test/xlir_product_result_generated.c"
#endif

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "xlir_product_result: actual=%lld expected=%lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
