/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_future_handle_lifetime_generated.c"

xfuture* xlir_make_future(void)
{
    return NULL;
}

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 7 ? 0 : 1;
}
