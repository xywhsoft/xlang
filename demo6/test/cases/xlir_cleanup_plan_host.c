#include "../../build/test/xlir_cleanup_plan_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 ) return 1;
    xlir_fn_1();
    return 0;
}
