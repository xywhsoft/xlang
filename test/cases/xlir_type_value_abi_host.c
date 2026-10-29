#include <stdint.h>

uint64_t xlir_type_external = UINT64_C(0xB3B1FA7F768AFEB4);

#include "../../build/test/xlir_type_value_abi_generated.c"

int main(void)
{
    uint64_t failed;
    if ( xlir_module_init() != 1 ) return 1;
    if ( xlir_type_int != UINT64_C(0xE99D3F7F03464F1C) ) return 2;
    if ( xlir_type_async != UINT64_C(0x4955F0967ED4EF16) ) return 3;
    if ( xlir_type_closure != UINT64_C(0x4955F0967ED4EF16) ) return 4;
    if ( xlir_type_identity(UINT64_C(0x730AFBEFBAD8CCE6)) !=
         UINT64_C(0x730AFBEFBAD8CCE6) ) return 5;
    if ( xlir_type_of_int(123) != UINT64_C(0xE99D3F7F03464F1C) ) return 6;
    if ( xlir_type_read_external() != UINT64_C(0xB3B1FA7F768AFEB4) ) return 7;
    failed = xlir_type_fail();
    return failed == 0 && xrtGetError() != NULL ? 0 : 8;
}
