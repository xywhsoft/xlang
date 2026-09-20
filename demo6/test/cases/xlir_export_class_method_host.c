#include "../../build/test/xlir_export_class_method_generated.c"

int main(void)
{
    xvalue* point = xl6_point_make_for_method(2, 3);
    xlir_product_t12 pair;
    int result = 0;
    if ( point == NULL ) return 1;
    if ( xl6_point_move(point, 5, 7) != 17 ) {
        result = 2;
        goto cleanup;
    }
    pair = xl6_point_pair(point);
    if ( pair.e0 != 7 || pair.e1 != 10 ) {
        result = 3;
        goto cleanup;
    }
    if ( xl6_point_local_value(point) != 19 ) result = 4;

cleanup:
    xrtValueRelease(point);
    return result;
}
