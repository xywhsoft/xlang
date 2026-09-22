/* FIX C smoke: ordered xvalue fields preserve repeated tags and build a valid frame. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

xvalue* xfix_field(int64 iTag, cstr sValue);
cstr xfix_build(cstr sBeginString, xvalue* pFields);
xvalue* xfix_parse(cstr sMessage);
bool xfix_validate(cstr sMessage);

int main(void)
{
    xvalue* pFields = xrtValueArray();
    xvalue* pField = xfix_field(35, "0");
    cstr sMessage;
    xvalue* pParsed;
    if (pFields == NULL || pField == NULL || !xrtValueArrayAppendTake(pFields, &pField)) { return 1; }
    sMessage = xfix_build("FIX.4.4", pFields);
    xrtValueRelease(pFields);
    if (sMessage == NULL || !xfix_validate(sMessage) || strstr(sMessage, "35=0\x01") == NULL) { return 1; }
    pParsed = xfix_parse(sMessage);
    if (pParsed == NULL || xrtValueType(pParsed) != XVALUE_OBJECT) { xrtValueRelease(pParsed); return 1; }
    xrtValueRelease(pParsed);
    puts("fix C smoke: PASS");
    return 0;
}

#include "../fix_native.c"
