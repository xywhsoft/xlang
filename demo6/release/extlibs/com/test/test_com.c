/* Windows COM smoke: verify IDispatch maps to current XRT xvalue values. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>

void* x_com_attach(void* pDispatch);
void* x_com_test_object(void);
int x_com_release(void* pHandle);
xvalue* x_com_invoke_array(void* pHandle, cstr sName, xvalue* pArgs);
xvalue* x_com_invoke_out(void* pHandle, cstr sName, xvalue* pArgs, xvalue* pByRefs);

static int fail(cstr sMessage)
{
    fprintf(stderr, "com C smoke failed: %s\n", sMessage);
    return 1;
}

int main(void)
{
    void* pObject;
    xvalue* pArgs;
    xvalue* pResult;
    xvalue* pOut;
    xvalue* pByRefs;
    int64 iValue = 0;

    pObject = x_com_attach(x_com_test_object());
    if (pObject == NULL) { return fail("test object creation"); }

    pArgs = xrtValueArray();
    if (pArgs == NULL || !xrtValueArrayAppendNew(pArgs, xrtValueInt(9)) ||
        !xrtValueArrayAppendNew(pArgs, xrtValueInt(4))) {
        xrtValueRelease(pArgs);
        x_com_release(pObject);
        return fail("argument construction");
    }
    pResult = x_com_invoke_array(pObject, "Sub", pArgs);
    xrtValueRelease(pArgs);
    if (pResult == NULL || !xrtValueGetInt(pResult, &iValue) || iValue != 5) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("array argument conversion");
    }
    xrtValueRelease(pResult);

    pArgs = xrtValueArray();
    pByRefs = xrtValueArray();
    if (pArgs == NULL || pByRefs == NULL ||
        !xrtValueArrayAppendNew(pArgs, xrtValueInt(41)) ||
        !xrtValueArrayAppendNew(pByRefs, xrtValueBool(true))) {
        xrtValueRelease(pArgs);
        xrtValueRelease(pByRefs);
        x_com_release(pObject);
        return fail("byref argument construction");
    }
    pOut = x_com_invoke_out(pObject, "Bump", pArgs, pByRefs);
    xrtValueRelease(pArgs);
    xrtValueRelease(pByRefs);
    if (pOut == NULL || xrtValueCount(pOut) != 2u ||
        !xrtValueGetInt(xrtValueArrayGet(pOut, 0u), &iValue) || iValue != 42 ||
        !xrtValueGetInt(xrtValueArrayGet(pOut, 1u), &iValue) || iValue != 42) {
        xrtValueRelease(pOut);
        x_com_release(pObject);
        return fail("byref result conversion");
    }
    xrtValueRelease(pOut);
    if (!x_com_release(pObject)) { return fail("release"); }
    puts("com C smoke: PASS");
    return 0;
}

#include "../x_com_artifact.c"
