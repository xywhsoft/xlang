#include <stdint.h>

int64_t xlir_test_time_value(void)
{
    return 123;
}

int64_t xlir_test_time_echo(int64_t value)
{
    return value + 1;
}
