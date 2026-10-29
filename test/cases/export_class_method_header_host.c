#include "export_class_method.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int64_t x = 0;
    int64_t y = 0;
    xvalue* point;
    XL_Class_Point* data;
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    point = xl6_point_make_for_method(2, 3);
    data = (XL_Class_Point*)xlValueData(point);
    if ( data == NULL || xl6_point_move(data, 5, 7) != 17 ) {
        result = 2;
        goto cleanup;
    }
    xl6_point_pair(data, &x, &y);
    if ( x != 7 || y != 10 ) {
        result = 3;
        goto cleanup;
    }
    if ( xl6_point_local_value(point) != 19 ) {
        result = 4;
    }

cleanup:
    xlValueRelease(point);
    xlUnit();
    return result;
}
