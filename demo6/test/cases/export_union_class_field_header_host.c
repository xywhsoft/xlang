#include "export_union_class_field.h"

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    XL_Union_Payload payload;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    payload = xl6_payload_make(42);
    if ( payload.integer != 42 ) {
        xlUnit();
        return 2;
    }
    result = xl6_payload_read(payload) == 42 ? 0 : 3;
    xlUnit();
    return result;
}
