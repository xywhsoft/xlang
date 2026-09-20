#include <string.h>

static int xlir_closure_string_capture_failed;

void expect_text(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 )
        xlir_closure_string_capture_failed = 1;
}

#include "../../build/test/xlir_closure_string_capture_generated.c"

int main(void)
{
    return !xlir_module_init() || xlir_closure_string_capture_failed ? 1 : 0;
}
