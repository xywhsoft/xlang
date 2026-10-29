#include "../../build/test/xlir_callsite_variadic_generated.c"

extern size_t xrtValueCount(const xvalue* value);

int64_t xvalue_vararg_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (int64_t)xrtValueCount(arg);
}

int64_t xvalue_kw_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (int64_t)xrtValueCount(arg);
}

int main(void)
{
    return xlir_callsite_variadic_probe() == 3020 ? 0 : 1;
}
