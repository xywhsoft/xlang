/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/cgen_assert_dynamic_message_runtime_generated.c"

extern const xerror* xrtGetError(void);
extern void xrtClearError(void);

int main(void)
{
    xlir_result_i64 failure = xlir_fn_1(false);
    xlir_result_i64 success;

    if ( failure.ok || xrtGetError() == NULL ) {
        return 1;
    }
    xrtClearError();
    success = xlir_fn_1(true);
    return success.ok && success.value == 23 && xrtGetError() == NULL ? 0 : 2;
}
