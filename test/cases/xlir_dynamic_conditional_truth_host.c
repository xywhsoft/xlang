#include "../../build/test/xlir_dynamic_conditional_truth_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    return xlir_dynamic_conditional_truth() == 50 ? 0 : 2;
}
