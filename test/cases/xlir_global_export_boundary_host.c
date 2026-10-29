#include "../../build/test/xlir_global_export_boundary_generated.c"

int main(void)
{
    /* Static initialization is part of GlobalExport itself: direct C access
     * must be correct before any function adapter triggers module init. */
    if ( xl6_counter != 1 ) return 1;
    if ( xl6_version != 7 ) return 2;
    if ( xl6_get_total() != 8 ) return 3;
    xl6_counter = 10;
    if ( xl6_get_total() != 17 ) return 4;
    xl6_set_counter(20);
    return xl6_counter == 20 ? 0 : 5;
}
