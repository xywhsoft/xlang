#include <stdint.h>
#include <string.h>

static int xlir_generic_expect_failed;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) xlir_generic_expect_failed = 1;
}

#include "../../build/test/xlir_generic_specialization_generated.c"

int main(void)
{
    if ( !xlir_module_init() || xlir_generic_expect_failed || s6 == NULL ||
         strcmp(s6, "ok") != 0 ) return 1;
    xrtFree(s6);
    s6 = NULL;
    return 0;
}
