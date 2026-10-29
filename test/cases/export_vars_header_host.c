#include "export_vars.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    if ( xl6_counter != 1 ) {
        result = 2;
    }
    if ( result == 0 && xl6_version != 7 ) {
        result = 3;
    }
    if ( result == 0 && xl6_get_total() != 8 ) {
        result = 4;
    }
    if ( result == 0 ) {
        xl6_counter = 10;
    }
    if ( result == 0 && xl6_get_total() != 17 ) {
        result = 5;
    }
    if ( result == 0 ) {
        xl6_set_counter(20);
    }
    if ( result == 0 && xl6_counter != 20 ) {
        result = 6;
    }
    xlUnit();
    return result;
}
