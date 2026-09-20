/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_catch_scalar_generated.c"

extern const xerror* xrtGetError(void);

int main(void)
{
    return xlir_fn_1() == 41 && xlir_fn_3() == 43 && xrtGetError() == NULL ? 0 : 1;
}
