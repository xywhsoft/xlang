/* Windows COM smoke: verify IDispatch maps to current XRT xvalue values. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../include/com.h"
#include "com_test_fixture.h"

#ifndef COINIT_APARTMENTTHREADED
#define COINIT_APARTMENTTHREADED 0x2
#endif

static DWORD WINAPI use_from_wrong_thread(void* handle)
{
    if (x_com_get_int(handle, "HasChild") != 0 ||
        strcmp(x_com_last_error(), "COM handle belongs to another thread") != 0 ||
        x_com_release(handle) != 0) {
        return 1;
    }
    return 0;
}

static int fail(cstr sMessage)
{
    fprintf(stderr, "com C smoke failed: %s\n", sMessage);
    return 1;
}

int main(void)
{
    void* pObject;
    void* dispatch;
    xvalue* pArgs;
    xvalue* pResult;
    xvalue* pOut;
    xvalue* pByRefs;
    int64 iValue = 0;
    xstrview text;
    const char* borrowed;
    HANDLE thread;
    DWORD thread_result;
    HRESULT apartment_result;

    if (!x_com_available()) { return fail("COM unavailable"); }
    apartment_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (apartment_result != S_OK) { return fail("availability check leaked COM initialization"); }
    CoUninitialize();

    dispatch = x_com_test_object();
    if (dispatch == NULL || !xrtMemDebugFailAfter(0u)) {
        return fail("allocation injection setup");
    }
    pObject = x_com_attach(dispatch);
    if (pObject != NULL || !xrtMemDebugFailTriggered() ||
        strcmp(x_com_last_error(), "out of memory") != 0) {
        xrtMemDebugFailClear();
        return fail("attach allocation failure");
    }
    xrtMemDebugFailClear();
    apartment_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (apartment_result != S_OK) { return fail("failed attach leaked COM initialization"); }
    CoUninitialize();
    pObject = x_com_attach(dispatch);
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

    if (x_com_get_int(pObject, "BigInt") != 1099511627776LL) {
        x_com_release(pObject);
        return fail("64-bit COM integer property");
    }

    pArgs = xrtValueArray();
    pByRefs = xrtValueArray();
    if (pArgs == NULL || pByRefs == NULL ||
        !xrtValueArrayAppendNew(pArgs, xrtValueInt(9)) ||
        !xrtValueArrayAppendNew(pArgs, xrtValueInt(4)) ||
        !xrtValueArrayAppendNew(pByRefs, xrtValueString((xstrview){"left", 4u})) ||
        !xrtValueArrayAppendNew(pByRefs, xrtValueString((xstrview){"", 0u}))) {
        xrtValueRelease(pArgs);
        xrtValueRelease(pByRefs);
        x_com_release(pObject);
        return fail("mixed argument construction");
    }
    pResult = x_com_invoke_named(pObject, "Sub", pArgs, pByRefs);
    xrtValueRelease(pArgs);
    xrtValueRelease(pByRefs);
    if (pResult == NULL || !xrtValueGetInt(pResult, &iValue) || iValue != 5) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("mixed named and positional arguments");
    }
    xrtValueRelease(pResult);

    pArgs = xrtValueArray();
    if (pArgs == NULL || !xrtValueArrayAppendNew(pArgs, xrtValueArray()) ||
        !xrtValueArrayAppendNew(pArgs, xrtValueInt(4))) {
        xrtValueRelease(pArgs);
        x_com_release(pObject);
        return fail("unsupported argument construction");
    }
    pResult = x_com_invoke_array(pObject, "Sub", pArgs);
    xrtValueRelease(pResult);
    xrtValueRelease(pArgs);
    if (strcmp(x_com_last_error(), "unsupported COM argument type") != 0) {
        x_com_release(pObject);
        return fail("unsupported COM argument rejection");
    }
    xrtClearError();

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

    pArgs = xrtValueArray();
    pResult = x_com_invoke_array(pObject, "Values", pArgs);
    xrtValueRelease(pArgs);
    if (pResult == NULL || xrtValueCount(pResult) != 2u ||
        !xrtValueGetInt(xrtValueArrayGet(pResult, 0u), &iValue) || iValue != 42 ||
        !xrtValueGetString(xrtValueArrayGet(pResult, 1u), &text) ||
        text.Size != 5u || memcmp(text.Data, "hello", 5u) != 0 ||
        strcmp(x_com_get_text(pObject, "Values"), "42,hello") != 0) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("one-dimensional SAFEARRAY");
    }
    xrtValueRelease(pResult);

    pArgs = xrtValueArray();
    pResult = x_com_invoke_array(pObject, "Grid", pArgs);
    xrtValueRelease(pArgs);
    if (pResult == NULL || xrtValueCount(pResult) != 2u ||
        xrtValueCount(xrtValueArrayGet(pResult, 0u)) != 2u ||
        !xrtValueGetInt(xrtValueArrayGet(xrtValueArrayGet(pResult, 1u), 1u), &iValue) ||
        iValue != 4) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("two-dimensional SAFEARRAY");
    }
    xrtValueRelease(pResult);

    pArgs = xrtValueArray();
    pResult = x_com_invoke_array(pObject, "Empty", pArgs);
    xrtValueRelease(pArgs);
    if (pResult == NULL || xrtValueType(pResult) != XVALUE_ARRAY || xrtValueCount(pResult) != 0u) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("empty SAFEARRAY");
    }
    xrtValueRelease(pResult);

    borrowed = x_com_get_text(pObject, "LongText");
    if (borrowed == NULL || strlen(borrowed) != 6006u ||
        strcmp(borrowed + 6000u, "\xE4\xBD\xA0\xE5\xA5\xBD") != 0) {
        x_com_release(pObject);
        return fail("long UTF-8 property text");
    }
    pArgs = xrtValueArray();
    pResult = x_com_invoke_array(pObject, "LongText", pArgs);
    xrtValueRelease(pArgs);
    if (pResult == NULL || !xrtValueGetString(pResult, &text) || text.Size != 6006u ||
        memcmp(text.Data + 6000u, "\xE4\xBD\xA0\xE5\xA5\xBD", 6u) != 0) {
        xrtValueRelease(pResult);
        x_com_release(pObject);
        return fail("long UTF-8 value text");
    }
    xrtValueRelease(pResult);

    pArgs = xrtValueArray();
    pResult = x_com_invoke_array(pObject, "Fail", pArgs);
    xrtValueRelease(pArgs);
    xrtValueRelease(pResult);
    if (strcmp(x_com_last_error(), "COM test failure") != 0) {
        x_com_release(pObject);
        return fail("COM exception description");
    }
    xrtClearError();

    if (x_com_create("\xFF") != NULL ||
        strcmp(x_com_last_error(), "ProgID conversion failed") != 0) {
        x_com_release(pObject);
        return fail("invalid UTF-8 ProgID");
    }
    xrtClearError();

    thread = CreateThread(NULL, 0, use_from_wrong_thread, pObject, 0, NULL);
    if (thread == NULL || WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
        !GetExitCodeThread(thread, &thread_result) || thread_result != 0) {
        if (thread != NULL) { CloseHandle(thread); }
        x_com_release(pObject);
        return fail("cross-thread handle rejection");
    }
    CloseHandle(thread);
    if (x_com_get_int(pObject, "HasChild") != 0) {
        x_com_release(pObject);
        return fail("owner-thread handle retained");
    }
    if (!x_com_release(pObject)) { return fail("release"); }
    apartment_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (apartment_result != S_OK) { return fail("handle release leaked COM initialization"); }
    CoUninitialize();
    puts("com C smoke: PASS");
    return 0;
}

#include "com_test_fixture.c"
