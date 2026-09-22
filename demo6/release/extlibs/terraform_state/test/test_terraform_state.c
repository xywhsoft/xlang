/* Terraform state C smoke: the same JSON/xvalue query API as xlang. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

xvalue* xtf_parse(cstr sText);
bool xtf_valid(cstr sText);
long long xtf_version(const xvalue* pState);
long long xtf_serial(const xvalue* pState);
cstr xtf_lineage(const xvalue* pState);
xvalue* xtf_addresses(const xvalue* pState);
xvalue* xtf_find(const xvalue* pState, cstr sAddress);
xvalue* xtf_attributes(const xvalue* pState, cstr sAddress, long long iIndex);
xvalue* xtf_output(const xvalue* pState, cstr sName);

static int fail(cstr sMessage)
{
    fprintf(stderr, "terraform_state C smoke failed: %s\n", sMessage);
    return 1;
}

static bool text_is(const xvalue* pValue, cstr sExpected)
{
    xstrview Text;
    size_t iSize = strlen(sExpected);

    return xrtValueGetString(pValue, &Text) && Text.Size == iSize &&
        memcmp(Text.Data, sExpected, iSize) == 0;
}

int main(void)
{
    cstr sState = "{\"version\":4,\"serial\":9,\"lineage\":\"c-smoke\","
        "\"outputs\":{\"answer\":{\"value\":42}},\"resources\":["
        "{\"type\":\"aws_instance\",\"name\":\"web\",\"instances\":["
        "{\"attributes\":{\"id\":\"i-c\"}}]}]}";
    xvalue* pState = xtf_parse(sState);
    xvalue* pAddresses;
    xvalue* pAttributes;
    xvalue* pOutput;
    int64 iAnswer;

    if (pState == NULL || !xtf_valid(sState) || xtf_valid("[]") ||
        xtf_version(pState) != 4 || xtf_serial(pState) != 9 ||
        strcmp(xtf_lineage(pState), "c-smoke") != 0) {
        xrtValueRelease(pState);
        return fail("state metadata");
    }
    pAddresses = xtf_addresses(pState);
    pAttributes = xtf_attributes(pState, "aws_instance.web", 0);
    pOutput = xtf_output(pState, "answer");
    if (pAddresses == NULL || xrtValueCount(pAddresses) != 1u ||
        !text_is(xrtValueArrayGet(pAddresses, 0u), "aws_instance.web") ||
        pAttributes == NULL || !text_is(xrtValueObjectGet(pAttributes, XRT_STR_LITERAL("id")), "i-c") ||
        pOutput == NULL || !xrtValueGetInt(xrtValueObjectGet(pOutput, XRT_STR_LITERAL("value")), &iAnswer) ||
        iAnswer != 42) {
        xrtValueRelease(pAddresses);
        xrtValueRelease(pAttributes);
        xrtValueRelease(pOutput);
        xrtValueRelease(pState);
        return fail("resource queries");
    }
    {
        xvalue* pMissing = xtf_find(pState, "missing");
        if (pMissing == NULL || xrtValueType(pMissing) != XVALUE_NULL) {
            xrtValueRelease(pMissing);
            xrtValueRelease(pAddresses);
            xrtValueRelease(pAttributes);
            xrtValueRelease(pOutput);
            xrtValueRelease(pState);
            return fail("missing resource");
        }
        xrtValueRelease(pMissing);
    }
    xrtValueRelease(pAddresses);
    xrtValueRelease(pAttributes);
    xrtValueRelease(pOutput);
    xrtValueRelease(pState);
    puts("terraform_state C smoke: PASS");
    return 0;
}
