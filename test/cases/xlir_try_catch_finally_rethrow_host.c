/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_catch_finally_rethrow_generated.c"

extern const xerror* xrtGetError(void);

int main(void)
{
    xlir_result_i64 result;
    if ( xlir_module_init() != 1 ) return 1;
    result = xlir_fn_1();
    if ( result.ok || xrtGetError() == NULL ) return 2;
    return xlir_fn_2() == 1 ? 0 : 3;
}
