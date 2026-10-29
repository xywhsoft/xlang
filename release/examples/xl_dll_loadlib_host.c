__declspec(dllexport) long long xl6_dll_loadlib_value(void)
{
    return 42;
}

#ifndef XL6_EXAMPLE_NO_STANDALONE_MAIN
#include <stdio.h>

int main(void)
{
    printf("xl_dll_loadlib_host=%lld\n", xl6_dll_loadlib_value());
    return xl6_dll_loadlib_value() == 42 ? 0 : 1;
}
#endif
