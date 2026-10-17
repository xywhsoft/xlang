#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_thread_local.h"
#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount && before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static void prefixes(void)
{
    for (int mode = 0; mode != 2; ++mode) {
        bool complete = false;
        for (size_t point = 0; point != 48; ++point) {
            void* state = mode ? xl6ThreadLocalCreateV1() : NULL;
            xvalue* input = mode ? xrtValueInt(7) : NULL;
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            xl6_foreign_error_scope scope = {0}; assert(xl6ForeignCallBegin(&scope));
            assert(xrtMemDebugFailAfter(point));
            void* made = mode ? NULL : xl6ThreadLocalCreateV1();
            if (mode) xl6ThreadLocalSetV1(state, input);
            bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            xl6_foreign_error_result result = xl6ForeignCallEnd(&scope);
            if (mode) {
                assert(hit ? !result.ok && !xl6ThreadLocalHasV1(state) : result.ok && xl6ThreadLocalHasV1(state));
                if (!hit) assert(xl6ThreadLocalRemoveV1(state));
            } else { assert(hit ? !made : made != NULL); xl6ThreadLocalDestroyV1(made); }
            xrtErrorFree(result.error); xrtClearError(); balanced(&before);
            xl6ThreadLocalDestroyV1(state); xrtValueRelease(input);
            if (!hit) { complete = true; printf("thread-local complete allocation prefix mode %d: %zu\n", mode, point); break; }
        }
        assert(complete);
    }
}
static void projections(void)
{
    xvalue* bytes = xrtValueBytes((xbytesview){(const uint8_t*)"a\0b", 3}); assert(bytes);
    xvalue* number = xrtValueInt(-128); assert(number);
    xvalue* overflow = xrtValueInt(128); assert(overflow);
    xrtSetErrorKind(XERR_RANGE); const xerror* error = xrtGetError(); assert(error);
    assert(xrtMemDebugFailAfter(0));
    xvalue* held = xl6ValueTryBytesV1(bytes, 42); assert(held == bytes); xrtValueRelease(held);
    assert(xl6ValueTryBytesV1(number, 42) == xrtValueNull());
    int64_t output = 1000;
    assert(xl6ValueTryReadIntegerV1(number, XL6_VALUE_ADMIT_I8, 42, &output) && output == -128);
    assert(!xl6ValueTryReadIntegerV1(overflow, XL6_VALUE_ADMIT_I8, 42, &output) && output == -128);
    assert(!xl6ValueTryReadIntegerV1(number, XL6_VALUE_ADMIT_U8, 42, &output) && output == -128);
    assert(!xl6ValueTryReadIntegerV1(bytes, XL6_VALUE_ADMIT_I64, 42, &output) && output == -128);
    assert(!xrtMemDebugFailTriggered() && xrtGetError() == error);
    xrtMemDebugFailClear(); xrtClearError();
    assert(xrtValueTypeIdBind(bytes, 99));
    assert(xl6ValueTryBytesV1(bytes, 42) == xrtValueNull());
    xrtValueRelease(bytes); xrtValueRelease(number); xrtValueRelease(overflow);
}
static bool collect(xrtownershipref ref, void* context)
{ *(xrtownershipref*)context = ref; return true; }
static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{
    (void)context;
    const xrtownershipadapterv1* result = xl6ThreadLocalOwnershipAdapterV1(ref);
    const XL6_NativeHandlePolicyV1* policy = xl6ThreadLocalHandlePolicyV1Get();
    if (!result) result = xrtValueHandleOwnershipAdapterV1(ref, policy->Ops, policy->Trace);
    return result ? result : xrtValueOwnershipAdapterV1(ref);
}
static bool resolveV2(xrtownershipref ref, void* context, const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation)
{ *preparation = NULL; *adapter = resolve(ref, context); return *adapter != NULL; }
static void cycle(void)
{
    void* raw = xl6ThreadLocalCreateV1(); assert(raw); void* state = raw;
    xvalue* handle = xl6NativeHandleTakeV1(&raw, xl6ThreadLocalHandlePolicyV1Get()); assert(handle && !raw);
    xvalue* root = xrtValueObject(); assert(root);
    assert(xrtValueObjectSet(root, (xstrview){"local", 5}, handle)); xrtValueRelease(handle);
    xl6ThreadLocalSetV1(state, root);
    xvalue* weak = xrtValueWeakRef(root); assert(weak);
    XL6_OwnershipPlan* plan = NULL;
    assert(xl6OwnershipPlanTryCreateV2(xrtValueOwnership(root), resolveV2, NULL, &plan) == XL6_OWNERSHIP_PLAN_ROOTED && plan);
    xrtValueRelease(root);
    assert(xl6OwnershipPlanTryRefresh(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
}
typedef struct Work { void* state; xevent ready, release; xvalue* weak; } Work;
static int32 worker(void* raw)
{
    Work* work = raw;
    xvalue* object = xrtValueObject(); assert(object);
    work->weak = xrtValueWeakRef(object); assert(work->weak);
    xl6ThreadLocalSetV1(work->state, object); xrtValueRelease(object);
    assert(xrtEventSet(&work->ready));
    return xrtEventWaitFor(&work->release, UINT64_C(10000000)) == XWAIT_OK ? 0 : 1;
}
static void remote(bool claimed)
{
    void* raw = xl6ThreadLocalCreateV1(); assert(raw); void* state = raw;
    xvalue* handle = xl6NativeHandleTakeV1(&raw, xl6ThreadLocalHandlePolicyV1Get()); assert(handle);
    xrtownershipref native = {0}; assert(xl6ThreadLocalHandlePolicyV1Get()->Trace(handle, collect, &native));
    const xrtownershipadapterv1* adapter = xl6ThreadLocalOwnershipAdapterV1(native); assert(adapter);
    Work work = {.state = state};
    assert(xrtEventInit(&work.ready, false, false) && xrtEventInit(&work.release, true, false));
    xthread* thread = xrtThreadCreate(worker, &work, 0); assert(thread);
    assert(xrtEventWaitFor(&work.ready, UINT64_C(10000000)) == XWAIT_OK);
    int token;
    if (claimed) {
        xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(adapter->Claim(native.Data, &token)); assert(xrtOwnershipScopeEnd(&freeze));
    } else {
        xl6ThreadLocalCloseV1(state); assert(xrtValueWeakRefExpired(work.weak));
        xrtValueRelease(handle); handle = NULL;
    }
    assert(xrtEventSet(&work.release));
    assert(xrtThreadWaitFor(thread, UINT64_C(10000000)) == XWAIT_OK && xrtThreadExitCode(thread) == 0);
    xrtThreadDestroy(thread);
    if (claimed) {
        assert(!xrtValueWeakRefExpired(work.weak));
        xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        adapter->Restore(native.Data, &token); assert(xrtOwnershipScopeEnd(&freeze));
        assert(!xl6ThreadLocalHasV1(state)); /* drains deferred exit after restore */
        assert(xrtValueWeakRefExpired(work.weak)); xrtValueRelease(handle);
    }
    xrtValueRelease(work.weak);
    assert(xrtEventUnit(&work.ready) && xrtEventUnit(&work.release));
}
typedef struct ActiveWork {
    void* state; xvalue* root;
    xevent entered, proceed, ended, exit;
} ActiveWork;
static void stallDrop(void* raw, void* context)
{
    assert(raw == context); ActiveWork* work = context;
    assert(xrtEventSet(&work->entered));
    assert(xrtEventWaitFor(&work->proceed, UINT64_C(10000000)) == XWAIT_OK);
}
static int32 activeWorker(void* context)
{
    ActiveWork* work = context; void* raw = work;
    static const xvaluehandleops hooks = {NULL, stallDrop, NULL, NULL};
    xvalue* prior = xrtValueHandleTake(&raw, &hooks, work); assert(prior && !raw);
    xl6ThreadLocalSetV1(work->state, prior); xrtValueRelease(prior);
    xl6ThreadLocalSetV1(work->state, work->root);
    assert(xrtEventSet(&work->ended));
    return xrtEventWaitFor(&work->exit, UINT64_C(10000000)) == XWAIT_OK ? 0 : 1;
}
static void activeCycle(void)
{
    void* raw = xl6ThreadLocalCreateV1(); assert(raw);
    ActiveWork work = {.state = raw};
    xvalue* handle = xl6NativeHandleTakeV1(&raw, xl6ThreadLocalHandlePolicyV1Get()); assert(handle);
    work.root = xrtValueObject(); assert(work.root);
    assert(xrtValueObjectSet(work.root, (xstrview){"local", 5}, handle)); xrtValueRelease(handle);
    xvalue* weak = xrtValueWeakRef(work.root); assert(weak);
    assert(xrtEventInit(&work.entered, false, false) && xrtEventInit(&work.proceed, true, false));
    assert(xrtEventInit(&work.ended, false, false) && xrtEventInit(&work.exit, true, false));
    xthread* thread = xrtThreadCreate(activeWorker, &work, 0); assert(thread);
    assert(xrtEventWaitFor(&work.entered, UINT64_C(10000000)) == XWAIT_OK);
    xrtValueRelease(work.root); /* State now owns the only non-plan root. */
    XL6_OwnershipPlan* plan = NULL;
    XL6_OwnershipPlanResult status = xl6OwnershipPlanTryCreateV2(
        xrtValueOwnership(work.root), resolveV2, NULL, &plan);
    /* The XRT handle destructor may still own a mutation scope: BUSY with no
     * output is valid. If freezing succeeds, the real activity pin is ROOTED.
     * Neither state may turn into unknown-node/ERROR or permit retirement. */
    assert((status == XL6_OWNERSHIP_PLAN_BUSY && !plan && !xrtGetError()) ||
        (status == XL6_OWNERSHIP_PLAN_ROOTED && plan));
    assert(xrtEventSet(&work.proceed));
    assert(xrtEventWaitFor(&work.ended, UINT64_C(10000000)) == XWAIT_OK);
    assert((plan ? xl6OwnershipPlanTryRefresh(plan) : xl6OwnershipPlanTryCreateV2(
        xrtValueOwnership(work.root), resolveV2, NULL, &plan)) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan) == XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    assert(xrtEventSet(&work.exit));
    assert(xrtThreadWaitFor(thread, UINT64_C(10000000)) == XWAIT_OK && xrtThreadExitCode(thread) == 0);
    xrtThreadDestroy(thread);
    assert(xrtEventUnit(&work.entered) && xrtEventUnit(&work.proceed));
    assert(xrtEventUnit(&work.ended) && xrtEventUnit(&work.exit));
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    void* warm = xl6ThreadLocalCreateV1(); assert(warm); xl6ThreadLocalDestroyV1(warm);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    prefixes(); projections(); cycle(); remote(false); remote(true); activeCycle();
    assert(!xrtGetError()); balanced(&before);
    puts("thread-local: owned values, native TLS indices, cycle retirement, remote close, claimed exit and OOM balanced");
    return 0;
}
