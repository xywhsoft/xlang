#include "export_class_method.h"

int main(void)
{
    int64_t x = 0;
    int64_t y = 0;
    XL_Class_Point point = xl6_point_make_for_method(2, 3);

    if ( xl6_point_move(&point, 5, 7) != 17 ) {
        return 1;
    }
    xl6_point_pair(&point, &x, &y);
    if ( x != 7 || y != 10 ) {
        return 2;
    }
    if ( xl6_point_local_value(point) != 19 ) {
        return 3;
    }
    return 0;
}
