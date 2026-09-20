#include "../../build/test/xlir_export_overload_scalar_generated.c"

int main(void)
{
    return xl6_export_add__i64(41) == 42 && xl6_export_add__bool(true) == 42 ? 0 : 1;
}
