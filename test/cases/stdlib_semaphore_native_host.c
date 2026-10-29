#include "src/runtime/xl6_semaphore_admission.h"
#include "src/runtime/xl6_semaphore.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { SEM_ID = 301, ARRAY_ID = 302, NULLABLE_ID = 303 };
static bool collect(xrtownershipref ref, void* context)
{ *(xrtownershipref*)context = ref; return true; }
static xrtownershipref ownerRef(xvalue* value)
{
    xrtownershipref ref = {0};
    xrtownershipref shell = xrtValueOwnership(value);
    assert(shell.Data && shell.Ops->Trace(shell.Data, collect, &ref));
    assert(ref.Data && xl6SemOwnershipAdapterV1(ref));
    return ref;
}
static void fakeDrop(void* raw, void* context) { (void)raw; (void)context; }
static void vectors(void)
{
    assert(!xl6SemCreateV1(0, 0, 1)); xrtClearError();
    assert(!xl6SemCreateV1(SEM_ID, -1, 1)); xrtClearError();
    assert(!xl6SemCreateV1(SEM_ID, 2, 1)); xrtClearError();
    assert(!xl6SemCreateV1(SEM_ID, 0, (int64_t)UINT32_MAX + 1)); xrtClearError();
    xvalue* sem = xl6SemCreateV1(SEM_ID, 1, 2); assert(sem);
    xvalue* alias = xrtValueClone(sem); assert(alias && xl6SemAdmitsV1(alias, SEM_ID));
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior = xrtGetError();
    assert(!xl6SemPostMultipleV1(alias, SEM_ID, 2));
    assert(xl6SemTryWaitV1(sem, SEM_ID) && !xl6SemTryWaitV1(alias, SEM_ID));
    assert(xl6SemPostV1(alias, SEM_ID) && xl6SemWaitV1(sem, SEM_ID));
    assert(!xl6SemWaitTimeoutV1(sem, SEM_ID, -1));
    assert(xl6SemPostV1(sem, SEM_ID) && xl6SemWaitTimeoutV1(alias, SEM_ID, INT64_MAX));
    assert(!xl6SemPostMultipleV1(sem, SEM_ID, 0));
    assert(!xl6SemPostMultipleV1(sem, SEM_ID, (int64_t)UINT32_MAX + 1));
    assert(!xl6SemTryWaitV1(sem, SEM_ID + 1) && !xl6SemTryWaitV1(NULL, SEM_ID));
    assert(xrtGetError() == prior);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SemAdmitsV1(sem, SEM_ID) && !xl6SemAdmitsV1(sem, SEM_ID + 1));
    assert(!xrtMemDebugFailTriggered() && xrtGetError() == prior); xrtMemDebugFailClear();
    xrtClearError();
    static const xvaluehandleops fakeOps = {NULL, fakeDrop, NULL, NULL};
    void* fakeData = (void*)(uintptr_t)1;
    xvalue* foreign = xrtValueHandleTake(&fakeData, &fakeOps, NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign, SEM_ID));
    assert(!xl6SemAdmitsV1(foreign, SEM_ID));
    assert(!xl6SemOwnershipAdapterV1(xrtValueOwnership(foreign)));
    xvalue* array = xl6SequenceSemCreateV1(1, ARRAY_ID, SEM_ID, SEM_ID, false);
    xvalue* same = xl6SequenceSemCreateV1(1, ARRAY_ID + 1, SEM_ID, SEM_ID, false);
    xvalue* nullable = xl6SequenceSemCreateV1(2, ARRAY_ID + 2, NULLABLE_ID, SEM_ID, true);
    assert(array && same && nullable);
    XL6_SequenceRoleV1 role;
    XL6_SequenceLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy = {0}, equivalent = {0}, optional = {0};
    assert(xl6SequenceSchemaCopyV1(array, &role, &layout, &policy));
    assert(xl6SequenceSchemaCopyV1(same, &role, &layout, &equivalent));
    assert(xl6SequenceSchemaCopyV1(nullable, &role, &layout, &optional));
    assert(policy.ops == xl6ArraySemAdmissionOpsV2Get() && policy.data != equivalent.data);
    assert(policy.ops->Equivalent(policy.data, equivalent.data));
    assert(!policy.ops->Equivalent(policy.data, optional.data));
    assert(policy.ops->Value(policy.data, sem) && !policy.ops->Value(policy.data, foreign));
    assert(!policy.ops->Value(policy.data, xrtValueNull()) && optional.ops->Value(optional.data, xrtValueNull()));
    assert(!policy.ops->Defaultable(policy.data) && optional.ops->Defaultable(optional.data));
    xl6SequenceAdmissionDropV3(&policy); xl6SequenceAdmissionDropV3(&equivalent); xl6SequenceAdmissionDropV3(&optional);
    xrtValueRelease(array); xrtValueRelease(same); xrtValueRelease(nullable); xrtValueRelease(foreign);
    xrtValueRelease(alias);
    xrtownershipref owner = ownerRef(sem);
    const xrtownershipadapterv1* adapter = xl6SemOwnershipAdapterV1(owner); assert(adapter);
    int token;
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Claim(owner.Data, &token));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6SemPostV1(sem, SEM_ID) && xl6SemTryWaitV1(sem, SEM_ID));
    assert(xrtOwnershipFreezeTryBegin(&freeze)); adapter->Clear(owner.Data, &token);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_TIMEOUT); prior = xrtGetError();
    assert(!xl6SemAdmitsV1(sem, SEM_ID) && !xl6SemPostV1(sem, SEM_ID));
    assert(adapter->Finish(owner.Data, &token) && adapter->Finish(owner.Data, &token));
    assert(xrtGetError() == prior); xrtClearError(); xrtValueRelease(sem);
}
typedef struct Waiter { const xvalue* sem; bool result; } Waiter;
static int32 waitWorker(void* raw)
{
    Waiter* waiter = raw;
    waiter->result = xl6SemWaitTimeoutV1(waiter->sem, SEM_ID, 500);
    assert(!xrtGetError()); return 0;
}
static void activeRelease(void)
{
    xvalue* sem = xl6SemCreateDefaultV1(SEM_ID); assert(sem);
    xvalue* weak = xrtValueWeakRef(sem); assert(weak);
    xrtownershipref owner = ownerRef(sem);
    Waiter waiter = {sem, true}; xthread* thread = xrtThreadCreate(waitWorker, &waiter, 0); assert(thread);
    bool busy = false;
    for (unsigned i = 0; i < 10000 && !busy; ++i) {
        xrtownershipscope freeze = {0};
        if (xrtOwnershipFreezeTryBegin(&freeze)) {
            busy = !xl6SemOwnershipAdapterV1(owner);
            assert(xrtOwnershipScopeEnd(&freeze));
        }
        if (!busy) xrtThreadYield();
    }
    assert(busy);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior = xrtGetError();
    assert(xrtMemDebugFailAfter(0)); assert(xl6SemAdmitsV1(sem, SEM_ID));
    assert(!xrtMemDebugFailTriggered() && xrtGetError() == prior);
    xrtMemDebugFailClear(); xrtClearError();
    xrtValueRelease(sem); assert(xrtValueWeakRefExpired(weak));
    assert(xrtThreadWaitFor(thread, UINT64_C(30000000)) == XWAIT_OK && !waiter.result);
    xrtThreadDestroy(thread); xrtValueRelease(weak);
}
static void failures(bool schema)
{
    size_t point;
    for (point = 0; point < 64; ++point) {
        assert(xrtMemDebugFailAfter(point));
        xvalue* value = schema ? xl6SequenceSemCreateV1(1, ARRAY_ID, SEM_ID, SEM_ID, false)
            : xl6SemCreateV1(SEM_ID, 0, 1);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) assert(!value && (!xrtGetError() || xrtErrorKind(xrtGetError()) == XERR_MEMORY));
        else assert(value && !xrtGetError());
        xrtClearError(); xrtValueRelease(value);
        if (!hit) break;
    }
    assert(point > 0 && point < 64);
    printf("Semaphore %s: complete allocation prefix %zu\n", schema ? "schema" : "owner", point);
}
typedef struct Concurrent { const xvalue* gate; bool producer; } Concurrent;
static int32 concurrentWorker(void* raw)
{
    const Concurrent* work = raw;
    for (unsigned i = 0; i < 10000; ++i) {
        if (work->producer) assert(xl6SemPostV1(work->gate, SEM_ID));
        else assert(xl6SemWaitV1(work->gate, SEM_ID));
    }
    assert(!xrtGetError()); return 0;
}
static void concurrentActivity(void)
{
    xvalue* gate = xl6SemCreateDefaultV1(SEM_ID); assert(gate);
    Concurrent work[8]; xthread* threads[8];
    for (unsigned i = 0; i < 8; ++i) {
        work[i] = (Concurrent){gate, i < 4};
        threads[i] = xrtThreadCreate(concurrentWorker, &work[i], 0); assert(threads[i]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        assert(xrtThreadWaitFor(threads[i], UINT64_C(30000000)) == XWAIT_OK);
        xrtThreadDestroy(threads[i]);
    }
    assert(!xl6SemTryWaitV1(gate, SEM_ID));
    xrtownershipref owner = ownerRef(gate); size_t refs = 0;
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(owner.Ops->Count(owner.Data, &refs) && refs == 1);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtValueRelease(gate);
    puts("Semaphore concurrent activity: 4 producers/4 consumers, 80000 native calls, no residual pins");
}
int main(int argc, char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    /* Instrumented providers cannot observe the old runtime's freeze gate.
     * TSan A/B focuses on actual simultaneous native calls, without any
     * concurrent freeze observer that could produce a false-positive edge. */
    if (argc == 2 && !strcmp(argv[1], "--race-stress-only")) concurrentActivity();
    else {
        assert(argc == 1);
        vectors(); activeRelease(); concurrentActivity(); failures(false); failures(true);
    }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("Semaphore resident core: genuine provenance, alias state, owned policy, phased finish, actual busy last-shell release and OOM balanced");
    return 0;
}
