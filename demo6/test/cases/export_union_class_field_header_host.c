#include "export_union_class_field.h"

int main(void)
{
    XL_Union_Payload payload = xl6_payload_make(19, 23);
    if ( payload.point.x != 19 ) {
        return 1;
    }
    if ( payload.point.y != 23 ) {
        return 2;
    }
    return xl6_payload_sum(payload) == 42 ? 0 : 3;
}
