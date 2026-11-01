/* 同一外部报文、帧校验和五行输出；默认无网络/文件。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_NUMBER_FLOAT
#include "xrt.h"
#include "../fix_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* frames[] = {
        "8=FIX.4.4\x01" "9=0008\x01" "35=0007\x01" "10=205\x01",
        "8=FIX.4.4\x01" "9=9\x01" "035=0007\x01" "10=110\x01",
        "8=FIX.4.4\x01" "9=0008\x01" "35=0007\x01" "10=000\x01"
    };
    unsigned accepted = 0, rejected = 0;
    for (size_t i = 0; i < sizeof(frames)/sizeof(*frames); ++i) {
        if (xfix_validate(frames[i])) ++accepted;
        else ++rejected;
        if (xfix_has_error()) return 1;
    }
    xvalue* result = xfix_parse(frames[0]);
    xvalue* fields = xrtValueObjectGet(result, XRT_STR_LITERAL("fields"));
    xvalue* field = xrtValueArrayGet(fields, 2);
    xstrview value = {0};
    bool ok = accepted == 1u && rejected == 2u && xrtValueCount(fields) == 4 &&
        xrtValueGetString(xrtValueObjectGet(field, XRT_STR_LITERAL("value")), &value) &&
        value.Size == 4u && memcmp(value.Data, "0007", 4u) == 0;
    if (ok) {
        printf("checked: 3\naccepted: %u\nrejected: %u\naccepted fields: 4\nvalue: %.*s\n",
            accepted, rejected, (int)value.Size, value.Data);
    }
    xrtValueRelease(result);
    return ok ? 0 : 1;
}

#include "../fix_native.c"
