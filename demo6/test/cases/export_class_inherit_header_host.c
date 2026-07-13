#include "../../lib/xrt/xrt.h"
#include "export_class_inherit.h"

int main(void)
{
    xvalue point;
    XL_Class_NamedPoint* data;
    int result = 0;

    if ( xrtInit() == NULL ) {
        return 1;
    }
    point = xl6_named_point_make(5, 7, 30);
    data = (XL_Class_NamedPoint*)xvoGetRecordData(point);
    if ( data == NULL || data->base.id != 5 ) {
        result = 2;
    } else if ( data->x + data->y != 37 ) {
        result = 3;
    } else if ( xl6_named_point_score(point) != 42 ) {
        result = 4;
    }
    xvoUnref(point);
    xrtUnit();
    return result;
}
