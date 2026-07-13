#include "lib/xrt/xrt.h"

int main(void)
{
    XL_Struct_AbiPair value;
    XL_Struct_AbiPair shifted;

    value.x = 1;
    value.y = 2;
    shifted = xl6_pair_shift(value);
    if ( shifted.x != 2 || shifted.y != 4 ) {
        return 1;
    }
    xl6_pair_reset(&shifted);
    if ( shifted.x != 0 || shifted.y != 0 ) {
        return 2;
    }
    xl6_pair_make(&shifted);
    return shifted.x == 3 && shifted.y == 4 ? 0 : 3;
}
