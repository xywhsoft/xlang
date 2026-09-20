#include "../../build/test/xlir_extern_global_function_generated.c"

int64_t host_counter = 40;

int main(void)
{
    if ( xl6_read_counter() != 40 ) return 1;
    if ( xl6_add_counter(2) != 42 ) return 2;
    return host_counter == 42 ? 0 : 3;
}
