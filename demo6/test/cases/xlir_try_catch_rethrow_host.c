/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_catch_rethrow_generated.c"

extern const xerror* xrtGetError(void);

int main(void)
{
    xlir_result_i64 result = xlir_fn_1();
    return !result.ok && xrtGetError() != NULL ? 0 : 1;
}
