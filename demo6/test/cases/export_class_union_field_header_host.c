#include "../../lib/xrt/xrt.h"
#include "export_class_union_field.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    xvalue box;
    XL_Class_Box* data;
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    box = xl6_box_make(42);
    data = (XL_Class_Box*)xvoGetRecordData(box);
    if ( data == NULL || data->payload.i != 42 ) {
        result = 2;
    } else if ( xl6_box_pick(box) != 42 ) {
        result = 3;
    }
    xvoUnref(box);
    xlUnit();
    return result;
}
