#include "export_vars.h"

int main(void)
{
    if ( xl6_counter != 1 ) {
        return 1;
    }
    if ( xl6_version != 7 ) {
        return 2;
    }
    if ( xl6_get_total() != 8 ) {
        return 3;
    }
    xl6_counter = 10;
    if ( xl6_get_total() != 17 ) {
        return 4;
    }
    xl6_set_counter(20);
    if ( xl6_counter != 20 ) {
        return 5;
    }
    return 0;
}
