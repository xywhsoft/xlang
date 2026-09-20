#include <stdint.h>

static int xlir_closure_capture_failed;

void expect_i64(int64_t actual, int64_t expected)
{
    if (actual != expected) xlir_closure_capture_failed = 1;
}

#include "../../build/test/xlir_closure_capture_generated.c"

int main(void)
{
    return !xlir_module_init() || xlir_closure_capture_failed ? 1 : 0;
}
