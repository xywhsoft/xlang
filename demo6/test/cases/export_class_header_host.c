#include "export_class.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    xvalue* point;
    XL_Class_Point* data;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    point = xl6_point_make(20, 22);
    data = (XL_Class_Point*)xlValueData(point);
    if ( data == NULL || data->x + data->y != 42 ) {
        xlValueRelease(point);
        xlUnit();
        return 2;
    }
    result = xl6_point_sum(point) == 42 ? 0 : 3;
    xlValueRelease(point);
    xlUnit();
    return result;
}
