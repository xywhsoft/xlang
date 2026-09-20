/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_await_future_i64_catch_generated.c"

xfuture* xlir_make_i64_future(void)
{
    return NULL;
}

int main(void)
{
    return xlir_module_init() == 1 && s3 == 7 && xrtGetError() == NULL ? 0 : 1;
}
