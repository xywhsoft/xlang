#include <stdint.h>
#include <stdlib.h>

#include "../../build/test/xlir_owned_expression_discard_generated.c"

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
