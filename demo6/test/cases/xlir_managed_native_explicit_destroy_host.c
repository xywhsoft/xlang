#include "xlir_managed_native_explicit_destroy_support.c"
#include "../../build/test/xlir_managed_native_explicit_destroy_generated.c"

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
