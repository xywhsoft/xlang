#include "export_c_abi_types.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    if ( xl6_echo_c_size((size_t)123) != (size_t)123 ) {
        result = 2;
    }
    if ( result == 0 && xl6_echo_c_ssize((xl6_ssize_t)-123) != (xl6_ssize_t)-123 ) {
        result = 3;
    }
    if ( result == 0 && xl6_echo_c_ptrdiff((ptrdiff_t)-77) != (ptrdiff_t)-77 ) {
        result = 4;
    }
    if ( result == 0 && xl6_not_c_bool(false) != true ) {
        result = 5;
    }
    if ( result == 0 && xl6_echo_c_wchar((wchar_t)65) != (wchar_t)65 ) {
        result = 6;
    }
    if ( result == 0 && xl6_echo_c_long((long)-321) != (long)-321 ) {
        result = 7;
    }
    xlUnit();
    return result;
}
