#include "export_class_union_field.h"

int main(void)
{
    XL_Class_Box box = xl6_box_make(42);
    if ( box.payload.i != 42 ) {
        return 1;
    }
    return xl6_box_pick(box) == 42 ? 0 : 2;
}
