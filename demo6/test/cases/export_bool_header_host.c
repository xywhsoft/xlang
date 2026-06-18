#include "export_bool.h"

int main(void)
{
    if ( !xl6_is_even(42) ) {
        return 1;
    }
    if ( xl6_is_even(41) ) {
        return 1;
    }
    return 0;
}
