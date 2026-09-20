#include <stdint.h>

int64_t xlir_native_add(int64_t left, int64_t right)
{
    return left + right;
}

#include "../../build/tmp/xlir_extern_scalar.c"

int main(void)
{
    return xlir_module_init() && xlir_fn_2(40) == 42 ? 0 : 1;
}
