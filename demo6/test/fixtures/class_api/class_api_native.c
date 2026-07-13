#include <stdint.h>
#include "xrt.h"

typedef struct XL_Class_____xl6__import__class__api__Point {
    int64_t x;
    int64_t y;
} XL_Class_____xl6__import__class__api__Point;

int64_t xl6_class_api_sum(xvalue point)
{
    XL_Class_____xl6__import__class__api__Point* data;

    data = (XL_Class_____xl6__import__class__api__Point*)xvoGetRecordData(point);
    if ( data == NULL ) {
        return 0;
    }
    return data->x + data->y;
}
