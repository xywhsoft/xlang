#include <windows.h>
#include <stdint.h>

static char op_test_message[] = "hello";
static int op_test_number = 42;
static float op_test_fraction = 1.5f;

long long op_test_memory_pid(void)
{
    return (long long)GetCurrentProcessId();
}

long long op_test_memory_addr(void)
{
    return (long long)(intptr_t)op_test_message;
}

long long op_test_number_addr(void)
{
    return (long long)(intptr_t)&op_test_number;
}

long long op_test_fraction_addr(void)
{
    return (long long)(intptr_t)&op_test_fraction;
}
