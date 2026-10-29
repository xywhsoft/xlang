#include "../../build/test/xlir_export_error_scalar_generated.c"

int main(void)
{
    return xlir_public_fail() == 0 && xrtGetError() != NULL ? 0 : 1;
}
