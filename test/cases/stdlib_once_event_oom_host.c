#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_sync_internal.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static bool fault(bool event, size_t point)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point));
    void* raw = event ? xl6EventCreateV1(false, false) : xl6OnceCreateV1();
    xvalue* value = raw ? xl6NativeHandleTakeV1(&raw,
        event ? xl6EventHandlePolicyV1Get() : xl6OnceHandlePolicyV1Get()) : NULL;
    bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    assert(!raw);
    if (hit) assert(!value);
    else assert(value);
    xerror* error = xrtTakeError();
    xrtValueRelease(value); xrtErrorFree(error); xrtClearError();
    balanced(&before);
    return !hit;
}

static bool failedCallback(void* environment) { (void)environment; return false; }
static bool goodCallback(void* environment) { (void)environment; return true; }
static bool declinedCallback(void* environment, bool* result)
{ (void)environment; *result = false; return true; }
static void callbackWithoutDiagnostic(void)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    const XL6_ClosureRuntimeV1* runtime = xl6ClosureRuntimeV1Get();
    XL6_Closure* bad = runtime->Create(NULL, (void (*)(void))failedCallback, NULL,
        NULL, NULL, NULL, NULL, 0, NULL);
    XL6_Closure* good = runtime->Create(NULL, (void (*)(void))goodCallback, NULL,
        NULL, NULL, NULL, NULL, 0, NULL);
    XL6_Closure* declined = runtime->Create(NULL, (void (*)(void))declinedCallback, NULL,
        NULL, NULL, NULL, NULL, 0, NULL);
    void* once = xl6OnceCreateV1();
    assert(bad && good && declined && once);
    xl6_foreign_error_scope scope = {0};
    assert(xl6ForeignCallBegin(&scope));
    assert(xrtMemDebugFailAfter(0));
    xl6OnceRunV1(once, bad);
    assert(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xl6_foreign_error_result result = xl6ForeignCallEnd(&scope);
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    assert(xl6ForeignCallBegin(&scope));
    assert(!xl6OnceTryRunV1(once, declined));
    result = xl6ForeignCallEnd(&scope);
    assert(result.ok); xrtErrorFree(result.error);
    assert(xl6ForeignCallBegin(&scope));
    xl6OnceRunV1(once, good);
    result = xl6ForeignCallEnd(&scope);
    assert(result.ok); xrtErrorFree(result.error);
    xl6OnceDestroyV1(once);
    runtime->Release(bad); runtime->Release(good); runtime->Release(declined);
    balanced(&before);
}

static bool refuseUnit(XL6_SyncOwner* owner)
{ (void)owner; xrtSetErrorKind(XERR_STATE); return false; }
static void checkedRetirement(void)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    assert(!xl6SyncRetirementSnapshotV1(NULL, 0));
    void* raw = xl6EventCreateV1(false, false);
    assert(raw);
    void* native = NULL;
    assert(xrtValueGetHandle(raw, &native, NULL, NULL));
    XL6_SyncOwner* owner = native;
    bool (*original)(XL6_SyncOwner*) = owner->unit;
    owner->unit = refuseUnit;
    xvalue* value = xl6NativeHandleTakeV1(&raw, xl6EventHandlePolicyV1Get());
    assert(value && !raw);
    xl6_foreign_error_scope scope = {0};
    assert(xl6ForeignCallBegin(&scope));
    xrtValueRelease(value);
    xl6_foreign_error_result result = xl6ForeignCallEnd(&scope);
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    XL_SyncRetirementV1 record;
    size_t count = 0;
    assert(xlSyncRetirementSnapshotCheckedV1(&record, 1, &count) && count == 1);
    assert(record.kind == XL_SYNC_EVENT_V1 && record.token);
    xvalue* recovery = NULL;
    assert(xlSyncRetirementAcquireCheckedV1(record.token, &recovery) && recovery);
    /* The registry owns the native state throughout refusal and recovery. */
    owner->unit = original;
    assert(xlSyncRetirementRetryCheckedV1(record.token));
    xrtValueRelease(recovery);
    assert(!xl6SyncRetirementSnapshotV1(NULL, 0));
    xrtClearError();
    balanced(&before);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (int event = 0; event != 2; ++event) {
        void* raw = event ? xl6EventCreateV1(false, false) : xl6OnceCreateV1();
        xvalue* value = xl6NativeHandleTakeV1(&raw,
            event ? xl6EventHandlePolicyV1Get() : xl6OnceHandlePolicyV1Get());
        assert(value && !raw); xrtValueRelease(value);
    }
    xrtClearError();
#ifdef _WIN32
    DWORD handlesBefore, handlesAfter;
    assert(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore));
#endif
    for (int event = 0; event != 2; ++event) {
        bool completed = false;
        for (size_t point = 0; point != 16; ++point) completed |= fault(event != 0, point);
        assert(completed); /* Include a prefix past the last construction allocation. */
    }
#ifdef _WIN32
    assert(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) && handlesBefore == handlesAfter);
#endif
    callbackWithoutDiagnostic();
    checkedRetirement();
    puts("once/event: 32 allocation prefixes, checked callback OOM and retryable retirement balanced");
    return 0;
}
