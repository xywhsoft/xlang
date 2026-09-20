#include <stdint.h>
#include <string.h>

static int xlir_generic_inferred_failed;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) xlir_generic_inferred_failed = 1;
}

void expect_text(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 )
        xlir_generic_inferred_failed = 1;
}

#include "../../build/test/xlir_generic_inferred_scalar_generated.c"

int main(void)
{
    if ( !xlir_module_init() || xlir_generic_inferred_failed ) return 1;
    return 0;
}
