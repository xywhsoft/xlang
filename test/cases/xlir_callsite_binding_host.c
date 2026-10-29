#include "../../build/test/xlir_callsite_binding_generated.c"

int main(void)
{
    return xlir_callsite_binding_probe() == 150 ? 0 : 1;
}
