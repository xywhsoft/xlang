/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_numeric_literals_generated.c"

#include <stdlib.h>

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) exit(1);
}

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
