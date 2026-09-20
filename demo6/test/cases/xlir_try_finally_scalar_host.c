/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_finally_scalar_generated.c"

extern const xerror* xrtGetError(void);

int main(void)
{
    xlir_result_i64 failure;

    if ( xlir_module_init() != 1 || xlir_fn_1() != 3 ) {
        return 1;
    }
    failure = xlir_fn_2();
    return !failure.ok && xlir_fn_3() == 11 && xrtGetError() != NULL ? 0 : 2;
}
