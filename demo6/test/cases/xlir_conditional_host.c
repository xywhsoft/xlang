#include "../../build/test/xlir_conditional_generated.c"

int main(void)
{
    return xlir_module_init() && xlir_fn_31() == 2 ? 0 : 1;
}
