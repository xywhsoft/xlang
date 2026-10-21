#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_NUMBER_FLOAT
#include "xrt.h"
#include "../fix_native.h"
#include "../fix_native.c"
#include <stdio.h>

typedef struct { int tag; const char *value; } order_field;

int main(void)
{
    const order_field order[] = {
        {35, "D"}, {49, "BUY_SIDE"}, {56, "SELL_SIDE"},
        {11, "client-order-42"}, {55, "XLANG"}, {54, "1"},
        {38, "100"}, {40, "2"}, {44, "12.50"}
    };
    xvalue *fields = xrtValueArray();
    xvalue *parsed = NULL;
    char *message = NULL;
    size_t i;
    int ok = 0;

    if (!fields) return 1;
    for (i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        xvalue *field = xfix_field(order[i].tag, order[i].value);
        if (!field || !xrtValueArrayAppendTake(fields, &field)) {
            xrtValueRelease(field);
            goto done;
        }
    }
    message = xfix_build("FIX.4.4", fields);
    if (!message || !xfix_validate(message)) goto done;
    parsed = xfix_parse(message);
    if (!parsed || xrtValueType(parsed) != XVALUE_OBJECT) goto done;

    {
        xvalue *all = xrtValueObjectGet(parsed, XRT_STR_LITERAL("fields"));
        xvalue *item = all ? xrtValueArrayGet(all, 2) : NULL;
        xvalue *value = item ? xrtValueObjectGet(item, XRT_STR_LITERAL("value")) : NULL;
        xvalue *tag = item ? xrtValueObjectGet(item, XRT_STR_LITERAL("tag")) : NULL;
        xstrview text;
        int64 number;
        const unsigned char *p;
        if (!value || !tag || !xrtValueGetString(value, &text) ||
            !xrtValueGetInt(tag, &number)) goto done;
        printf("FIX message: ");
        for (p = (const unsigned char*)message; *p; ++p) putchar(*p == 1 ? '|' : *p);
        putchar('\n');
        puts("valid: true");
        printf("message type: %.*s\n", (int)text.Size, text.Data);
        printf("first application tag: %lld\n", (long long)number);
        puts("fix example: PASS");
        ok = 1;
    }

done:
    xrtValueRelease(parsed);
    xrtValueRelease(fields);
    xrtFree(message);
    return ok ? 0 : 1;
}
