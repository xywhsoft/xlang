#include "../../build/test/xlir_extern_global_boundary_generated.c"

int64_t host_counter = 40;
static int host_failed = 0;

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) host_failed = 1;
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( host_failed ) return 2;
    return host_counter == 42 ? 0 : 3;
}
