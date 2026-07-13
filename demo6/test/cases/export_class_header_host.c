#include "../../lib/xrt/xrt.h"
#include "export_class.h"

int main(void)
{
    xvalue point;
    XL_Class_Point* data;
    int result;

    if ( xrtInit() == NULL ) {
        return 1;
    }
    point = xl6_point_make(20, 22);
    data = (XL_Class_Point*)xvoGetRecordData(point);
    if ( data == NULL || data->x + data->y != 42 ) {
        xvoUnref(point);
        xrtUnit();
        return 2;
    }
    result = xl6_point_sum(point) == 42 ? 0 : 3;
    xvoUnref(point);
    xrtUnit();
    return result;
}
