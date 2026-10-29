/* 与 tag_gate.xl 相同的字节、错误处理和五行固定输出。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_NUMBER_FLOAT
#include "xrt.h"
#include "../fix_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* received = "8=FIX.4.4\x01" "9=0008\x01" "35=0007\x01" "10=205\x01";
    const char* malformed = "8=FIX.4.4\x01" "9=9\x01" "035=0007\x01" "10=110\x01";
    xvalue* message = xfix_parse(received);
    xvalue* fields = xrtValueObjectGet(message,XRT_STR_LITERAL("fields"));
    xvalue* field = xrtValueArrayGet(fields,2);
    xstrview text;
    bool ok = xfix_validate(received) && xrtValueGetString(
        xrtValueObjectGet(field,XRT_STR_LITERAL("value")),&text) && text.Size == 4 &&
        memcmp(text.Data,"0007",4) == 0;
    if (ok) { puts("valid=true"); printf("value=%.*s\n",(int)text.Size,text.Data); }
    bool rejected = !xfix_validate(malformed) && !xfix_has_error();
    if (ok && rejected) puts("padded_tag=false");
    xvalue* bad = xfix_parse(malformed);
    bool caught = xrtValueType(bad) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    xrtValueRelease(bad);
    ok = ok && rejected && caught && xfix_validate(received);
    if (ok) puts("caught=true\nrestored=true");
    xrtValueRelease(message);
    return ok ? 0 : 1;
}

#include "../fix_native.c"
