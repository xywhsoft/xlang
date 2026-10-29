#include "export_bool.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    if ( !xl6_is_even(42) ) {
        result = 2;
    }
    if ( result == 0 && xl6_is_even(41) ) {
        result = 3;
    }
    xlUnit();
    return result;
}
