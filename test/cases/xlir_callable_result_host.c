#include "../../build/test/xlir_callable_result.c"
#include "../../build/test/xlir_callable_result.h.bind.h"
#include <assert.h>
#include <string.h>
static RESULT_TYPE preparedNative, outputNative;
static xrtcallresult preparedFrame, outputFrame;
static xrtcallresult* activeFrame;
static xvalue* rawValue;
static xlir_closure native;
static xerror *primary, *secondary, *tertiary;
static unsigned currentKind, calls, drops;
static int inBridge, raiseDrops;
static void handle_drop(void* handle, void* data)
{
    assert(handle == &drops); ++drops;
    if (inBridge && activeFrame) assert(xrtCallResultCount(activeFrame) == 0);
    if (raiseDrops) xrtSetError(data ? tertiary : secondary);
}
static const xvaluehandleops handleOps = {NULL, handle_drop, NULL, NULL};
static xvalue* handle_value(int second)
{
    void* handle = &drops;
    xvalue* value = xrtValueHandleTake(&handle, &handleOps, second ? &calls : NULL);
    assert(value && !handle); return value;
}
static bool native_entry(void* data, RESULT_TYPE* output)
{
    (void)data; ++calls;
    *output = preparedNative; memset(&preparedNative, 0, sizeof(preparedNative));
    if (currentKind == 1 || currentKind == 2) { xrtSetError(primary); return false; }
    return true;
}
static bool raw_entry(void* data, const xrtcallframe* frame, xrtcallresult* results)
{
    (void)data; assert(frame && !frame->ArgumentCount); ++calls;
    /* XRT may supply a temporary frame whose lifetime ends before Invoke
     * returns; never retain that borrowed address in a destructor context. */
    assert(xrtCallResultMove(results, &preparedFrame));
    return currentKind != 8;
}
int result_init(void)
{
    xrtcallable* callable;
    if (!xlir_module_init()) return 0;
    primary = xrtErrorCreate(XERR_STATE, "result", 88, "primary body error");
    secondary = xrtErrorCreate(XERR_STATE, "result", 77, "secondary destructor");
    tertiary = xrtErrorCreate(XERR_STATE, "result", 99, "tertiary destructor");
    assert(primary && secondary && tertiary);
    callable = xrtCallableCreate(NULL, raw_entry, NULL, NULL); assert(callable);
    rawValue = xrtValueCallableTake(&callable); assert(rawValue && !callable);
    memset(&native, 0, sizeof(native)); native.invoke = (void (*)(void))native_entry;
    return 1;
}
static void assert_native(RESULT_TYPE* result)
{
    int64_t answer = 0;
    assert(result->e0.Data && result->e0.Size==4 && !memcmp(result->e0.Data, "text", 4) && result->e1 && result->e2 && result->e3);
    assert(((bool (*)(void*, int64_t*))result->e3->invoke)(result->e3->environment, &answer) && answer == 42);
}
void result_language_check(void)
{
    RESULT_TYPE result = {0};
    assert(((bool (*)(void*, RESULT_TYPE*))RESULT_RESTORED->invoke)(RESULT_RESTORED->environment, &result));
    assert_native(&result); RESULT_DROP(&result); assert(!xrtGetError());
}
void result_prepare(unsigned kind)
{
    xvalue* values[5] = {0}; unsigned i, count;
    currentKind = kind; calls = drops = 0; raiseDrops = kind == 2 || (kind >= 4 && kind <= 8);
    activeFrame = NULL;
    if (kind < 3) {
        preparedNative.e0 = xlir_string_from_utf8("text",4); preparedNative.e1 = handle_value(0);
        preparedNative.e2 = xrtErrorRef(primary);
        preparedNative.e3 = xlir_xvalue_project_closure_try_copy(RESULT_ANSWER, (void (*)(void))RESULT_ANSWER_ENTRY);
        assert(preparedNative.e0.Data && preparedNative.e2 && preparedNative.e3);
    } else if (kind < 9) {
        values[0] = xrtValueString(xrtStrView("text")); values[1] = handle_value(0);
        values[2] = kind == 4 ? xrtValueInt(1) : xl6ErrorValueRefV1(primary, RESULT_ERROR_TYPE);
        values[3] = kind == 5 ? xrtValueInt(2) : xrtValueRetain(RESULT_ANSWER);
        count = kind == 6 || kind == 7 ? 5 : 4;
        if (count == 5) values[4] = kind == 7 ? handle_value(1) : xrtValueInt(3);
        for (i = 0; i < count; ++i) assert(values[i] && xrtCallResultSetTake(&preparedFrame, i, &values[i]) && !values[i]);
    }
}
bool result_run(unsigned kind)
{
    bool ok; xrtcallframe frame = {0};
    inBridge = 1;
    if (kind < 3) {
        activeFrame = &outputFrame; ok = RESULT_FORWARD(&native, &frame, &outputFrame);
    } else if (kind == 9) {
        ok = xrtValueInvoke(RESULT_MANY, &frame, &outputFrame);
    } else ok = RESULT_ENTRY(&rawValue, &outputNative);
    inBridge = 0; activeFrame = NULL;
    if (!ok) {
        assert(!outputNative.e0.Data && !outputNative.e0.Size && !outputNative.e1 && !outputNative.e2 && !outputNative.e3);
        assert(xrtCallResultCount(&outputFrame) == 0);
    }
    return ok;
}
void result_finish(unsigned kind, bool ok)
{
    if (ok && kind == 3) assert_native(&outputNative);
    else if (ok) {
        assert(xrtCallResultCount(&outputFrame) == (kind == 9 ? 6u : 4u));
        if (kind == 9) {
            unsigned i; int64_t value;
            for (i = 0; i < 6; ++i) assert(xrtValueGetInt(xrtCallResultGet(&outputFrame, i), &value) && value == (int64_t)i + 1);
        }
    }
    raiseDrops = 0;
    RESULT_DROP(&outputNative); RESULT_DROP(&preparedNative);
    xrtCallResultUnit(&preparedFrame); xrtCallResultUnit(&outputFrame);
    assert(drops == (kind == 9 ? 0u : kind == 7 ? 2u : 1u));
    assert(kind == 9 || calls == 1);
}
void result_fini(void)
{
    xrtValueRelease(rawValue); xrtErrorFree(primary); xrtErrorFree(secondary); xrtErrorFree(tertiary);
    xlir_module_fini();
}
