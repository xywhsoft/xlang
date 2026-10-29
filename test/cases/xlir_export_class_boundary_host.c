#include "../../build/test/xlir_export_class_boundary_generated.c"

int main(void)
{
    xvalue* point = xl6_point_make(19, 23);
    int result;
    if ( point == NULL ) return 1;
    result = xl6_point_sum(point) == 42 ? 0 : 2;
    xrtValueRelease(point);
    return result;
}
