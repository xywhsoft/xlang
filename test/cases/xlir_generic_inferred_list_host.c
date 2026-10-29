#include <stdint.h>

static int xlir_generic_list_failed;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) xlir_generic_list_failed = 1;
}

#include "../../build/test/xlir_generic_inferred_list_generated.c"

int main(void)
{
    if ( !xlir_module_init() || xlir_generic_list_failed ) return 1;
    return 0;
}
