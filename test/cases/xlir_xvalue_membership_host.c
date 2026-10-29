#include <stdint.h>
#include <stdlib.h>

#include "../../build/test/xlir_xvalue_membership_generated.c"

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    int i;
    for ( i = 0; i < 3; ++i ) {
        if ( !xlir_module_init() ) return 1;
        xlir_module_fini();
        xlir_module_fini();
    }
    return 0;
}
