#include "export_class_inherit.h"

int main(void)
{
    XL_Class_NamedPoint point = xl6_named_point_make(5, 7, 30);
    if ( point.base.id != 5 ) {
        return 1;
    }
    if ( point.x + point.y != 37 ) {
        return 2;
    }
    return xl6_named_point_score(point) == 42 ? 0 : 3;
}
