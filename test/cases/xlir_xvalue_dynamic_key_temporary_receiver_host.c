#include <stdint.h>

#include "../../build/test/xlir_xvalue_dynamic_key_temporary_receiver_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    return xlir_dynamic_key_temporary_receiver() == INT64_C(42) ? 0 : 2;
}
