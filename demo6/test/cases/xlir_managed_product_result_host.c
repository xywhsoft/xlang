#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../build/test/xlir_managed_product_result_generated.c"

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "xlir_managed_product_result: actual=%s expected=%s\n",
                actual != NULL ? actual : "<null>",
                expected != NULL ? expected : "<null>");
        exit(1);
    }
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
