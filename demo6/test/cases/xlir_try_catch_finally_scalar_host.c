#include "../../build/test/xlir_try_catch_finally_scalar_generated.c"

extern const xerror* xrtGetError(void);

int main(void)
{
    xlir_result_i64 escaped;
    if ( xlir_module_init() != 1 ) return 1;
    if ( xlir_fn_1() != 11 ) return 2;
    escaped = xlir_fn_2();
    if ( escaped.ok || xrtGetError() == NULL ) return 3;
    return xlir_fn_3() == 111 ? 0 : 4;
}
