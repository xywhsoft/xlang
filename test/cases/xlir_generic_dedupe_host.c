#include <stdint.h>

static int xlir_generic_dedupe_failed;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) xlir_generic_dedupe_failed = 1;
}

#include "../../build/test/xlir_generic_dedupe_generated.c"

int main(void)
{
    return !xlir_module_init() || xlir_generic_dedupe_failed;
}
