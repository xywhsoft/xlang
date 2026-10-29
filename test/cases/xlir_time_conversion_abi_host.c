#include <stdint.h>

#include "../../build/test/xlir_time_conversion_abi_generated.c"

#include "xlir_time_conversion_abi_support.c"

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
