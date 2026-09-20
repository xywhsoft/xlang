#include <stdint.h>

#include "../../build/test/xlir_u64_async_closure_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 ) return 1;
    if ( xlir_u64_async_value != 9223372036854775815ULL ) return 2;
    if ( xlir_u64_closure_value != 18446744073709551614ULL ) return 3;
    return xlir_u64_cell_value == UINT64_MAX ? 0 : 4;
}
