#include "../../build/test/xlir_export_error_void_generated.c"

int main(void)
{
    xrtClearError();
    xlir_public_void_fail();
    return xrtGetError() != NULL ? 0 : 1;
}
