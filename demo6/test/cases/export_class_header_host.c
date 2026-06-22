#include "export_class.h"

int main(void)
{
    XL_Class_Point point = xl6_point_make(20, 22);
    if ( point.x + point.y != 42 ) {
        return 1;
    }
    return xl6_point_sum(point) == 42 ? 0 : 2;
}
