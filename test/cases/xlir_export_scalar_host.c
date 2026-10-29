#include "../../build/test/xlir_export_scalar_generated.c"

int main(void)
{
    return xlir_public_add(20, 22) == 42 ? 0 : 1;
}
