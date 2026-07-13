#include "export_union_class_field.h"

int main(void)
{
    XL_Union_Payload payload = xl6_payload_make(42);
    if ( payload.integer != 42 ) {
        return 1;
    }
    return xl6_payload_read(payload) == 42 ? 0 : 2;
}
