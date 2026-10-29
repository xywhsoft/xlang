#include <stdbool.h>

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    return xlir_dynamic_u64_arithmetic_probe() ? 0 : 2;
}
