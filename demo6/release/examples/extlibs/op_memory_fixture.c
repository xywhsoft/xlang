#include <windows.h>
#include <stdint.h>

static char op_example_text[] = "xlang-op";

long long op_example_pid(void)
{
    return (long long)GetCurrentProcessId();
}

long long op_example_text_address(void)
{
    return (long long)(intptr_t)op_example_text;
}
