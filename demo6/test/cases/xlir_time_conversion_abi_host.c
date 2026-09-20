#include <stdint.h>

#ifdef XLIR_USE_LEGACY_GENERATED
#include "../../build/test/xlir_time_conversion_abi_legacy.c"
#else
#include "../../build/test/xlir_time_conversion_abi_generated.c"
#endif

#include "xlir_time_conversion_abi_support.c"

#ifndef XLIR_USE_LEGACY_GENERATED
int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
#endif
