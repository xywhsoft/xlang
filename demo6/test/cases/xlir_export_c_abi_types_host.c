#include "../../build/test/xlir_export_c_abi_types_generated.c"

int main(void)
{
    if ( xl6_echo_c_size((size_t)123) != (size_t)123 ) return 1;
    if ( xl6_echo_c_ssize((intptr_t)-123) != (intptr_t)-123 ) return 2;
    if ( xl6_echo_c_ptrdiff((ptrdiff_t)-77) != (ptrdiff_t)-77 ) return 3;
    if ( xl6_not_c_bool(false) != true ) return 4;
    if ( xl6_echo_c_wchar((wchar_t)65) != (wchar_t)65 ) return 5;
    if ( xl6_echo_c_long((long)-321) != (long)-321 ) return 6;
    return 0;
}
