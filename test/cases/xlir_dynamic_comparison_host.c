#include "../../build/test/xlir_dynamic_comparison_generated.c"

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
