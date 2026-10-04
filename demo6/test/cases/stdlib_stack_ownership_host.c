#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <time.h>
#endif

#define REF_ARGS UINT64_C(102),UINT64_C(202),3,sizeof(xvalue*),xrtTypeValue()
#define TEXT_ARGS UINT64_C(101),UINT64_C(201),2,sizeof(xstrview),xrtTypeStringView()
#define FLOAT_ARGS UINT64_C(103),UINT64_C(203),1,sizeof(double),xrtTypeFloat64()
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
    assert(!xrtGetError());
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{
    (void)context;
    const xrtownershipadapterv1* adapter = xl6StackOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static bool admit(xrtownershipref ref, void* context) { return resolve(ref,context) != NULL; }
static void shape(xvalue* value, size_t nodes, size_t edges, size_t roots)
{
    xrtownershipscope freeze = {0}; xrtownershipsnapshot* snapshot = NULL;
    xrtownershipref root = xrtValueOwnership(value); xrtownershipresult result = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,NULL,&snapshot));
    result.NodeCount = xrtOwnershipSnapshotNodeCount(snapshot);
    for (size_t index = 0; index < result.NodeCount; ++index) {
        xrtownershipnode node;
        assert(xrtOwnershipSnapshotNode(snapshot,index,&node));
        result.EdgeCount += node.InternalCount;
        result.ExternalRootCount += node.StrongCount > node.InternalCount;
    }
    if (result.NodeCount != nodes || result.EdgeCount != edges || result.ExternalRootCount != roots)
        fprintf(stderr,"shape actual=%zu/%zu/%zu expected=%zu/%zu/%zu\n",
            result.NodeCount,result.EdgeCount,result.ExternalRootCount,nodes,edges,roots);
    assert(result.NodeCount == nodes && result.EdgeCount == edges && result.ExternalRootCount == roots);
    xrtOwnershipSnapshotDestroy(snapshot); assert(xrtOwnershipScopeEnd(&freeze));
}
static xvalue* cycle(xvalue** weak)
{
    xvalue* value = xl6StackCreateV1(REF_ARGS); assert(value);
    *weak = xrtValueWeakRef(value); assert(*weak);
    assert(xl6StackPushV1(value,REF_ARGS,&value) && xl6StackPushV1(value,REF_ARGS,&value));
    xrtValueRelease(value); /* Only the TWO actual native slots own this shell. */
    shape(value,2,3,0); return value;
}
static void retire(xvalue* value)
{
    XL6_OwnershipPlan* plan = NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
}
static void vectors(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* weak = NULL; xvalue* value = cycle(&weak);
    XL6_OwnershipPlan* plan = NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) == XL6_OWNERSHIP_PLAN_READY);
    xvalue* blocked = xrtValueWeakRefLock(weak);
    assert(blocked && xrtValueType(blocked) == XVALUE_NULL && !xrtGetError()); xrtValueRelease(blocked);
    /* Existing strong owners can still copy/read/mutate during semantic
     * finalization. Claim is weak/raw quarantine, not a ban on strong use. */
    xvalue* external = xrtValueRetain(value); assert(external);
    assert(xl6StackPushV1(external,REF_ARGS,&external));
    assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6StackSizeV1(external,REF_ARGS,false) == 3);
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
    xvalue* promoted = xrtValueWeakRefLock(weak); assert(promoted == external);
    xrtValueRelease(promoted); xrtValueRelease(external); retire(value);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);

    xrtMemDebugSnapshot(&before);
    value = cycle(&weak);
    /* XRT DeepClone creates another real shell whose Clone operation owns
     * the SAME native wrapper. That wrapper is counted/traced only once. */
    external = xrtValueDeepClone(value); assert(external && external != value);
    shape(value,2,3,1); /* External shell is outside this traversal, roots wrapper. */
    assert(xl6StackSizeV1(external,REF_ARGS,false) == 2);
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) == XL6_OWNERSHIP_PLAN_ROOTED);
    xrtValueRelease(external);
    assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);

    xrtMemDebugSnapshot(&before);
    xvalue* a = xl6StackCreateV1(REF_ARGS); xvalue* b = xl6StackCreateV1(REF_ARGS);
    xvalue* wa = xrtValueWeakRef(a); xvalue* wb = xrtValueWeakRef(b); assert(a && b && wa && wb);
    assert(xl6StackPushV1(a,REF_ARGS,&b) && xl6StackPushV1(b,REF_ARGS,&a));
    xrtValueRelease(a); xrtValueRelease(b); shape(a,4,4,0); retire(a);
    assert(xrtValueWeakRefExpired(wa) && xrtValueWeakRefExpired(wb));
    xrtValueRelease(wa); xrtValueRelease(wb); balanced(&before);

    xrtMemDebugSnapshot(&before);
    value = xl6StackCreateV1(TEXT_ARGS); xstrview text = {"a\0b",3}; assert(value);
    assert(xl6StackPushV1(value,TEXT_ARGS,&text)); shape(value,2,1,1); xrtValueRelease(value);
    value = xl6StackCreateV1(FLOAT_ARGS); double number = 1.0; assert(value);
    assert(xl6StackPushV1(value,FLOAT_ARGS,&number)); shape(value,2,1,1); xrtValueRelease(value);
    balanced(&before);
}
static unsigned opaqueDrops;
static void opaqueDrop(void* raw, void* context) { (void)raw; (void)context; ++opaqueDrops; }
static const xvaluehandleops opaqueOps = {NULL,opaqueDrop,NULL,NULL};
static void refusal(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* weak = NULL; xvalue* value = cycle(&weak); int raw = 7; void* handle = &raw;
    xvalue* unknown = xrtValueHandleTake(&handle,&opaqueOps,NULL); assert(unknown);
    assert(xl6StackPushV1(value,REF_ARGS,&unknown)); xrtValueRelease(unknown);
    XL6_OwnershipPlan* plan = NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) == XL6_OWNERSHIP_PLAN_ERROR);
    assert(!plan && !opaqueDrops); xrtClearError();
    xvalue* promoted = xrtValueWeakRefLock(weak); assert(promoted == value); xrtValueRelease(promoted);
    xl6StackClearV1(value,REF_ARGS); assert(opaqueDrops == 1 && xrtValueWeakRefExpired(weak));
    xrtValueRelease(weak); balanced(&before);
}
static void fault(const char* stage, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* weak = NULL; xvalue* value = cycle(&weak); XL6_OwnershipPlan* plan = NULL;
    bool create = !strcmp(stage,"create"), commit = !strcmp(stage,"commit");
    assert(create || commit || !strcmp(stage,"refresh"));
    if (!create) assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) == XL6_OWNERSHIP_PLAN_READY);
    assert(xrtMemDebugFailAfter(point));
    XL6_OwnershipPlanResult result = create ? xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) :
        commit ? xl6OwnershipPlanTryCommit(plan) : xl6OwnershipPlanTryRefresh(plan);
    bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? result == XL6_OWNERSHIP_PLAN_ERROR : result == XL6_OWNERSHIP_PLAN_READY);
    if (hit) {
        assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xerror* failure = xrtTakeError(); assert(xl6StackSizeV1(value,REF_ARGS,false) == 2);
        if (plan) assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
        assert(!xrtGetError()); xrtSetErrorTake(failure); xrtClearError(); retire(value);
    } else {
        if (!commit) assert(xl6OwnershipPlanTryCommit(plan) == XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY);
    }
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);
    printf("stack graph %s point=%zu hit=%d balanced\n",stage,point,hit);
}

typedef struct Gate {
    xatomic32 entered, proceed, done;
    xvalue* stack;
    xvalue* target;
    unsigned mode, drops;
} Gate;
static void yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}
static void await(const xatomic32* flag)
{
#ifdef _WIN32
    ULONGLONG start = GetTickCount64();
#else
    struct timespec start, now; assert(!clock_gettime(CLOCK_MONOTONIC,&start));
#endif
    while (!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)) {
#ifdef _WIN32
        assert(GetTickCount64()-start < 10000);
#else
        assert(!clock_gettime(CLOCK_MONOTONIC,&now) && now.tv_sec-start.tv_sec < 10);
#endif
        yield();
    }
}
static bool leafTrace(const xvalue* value, xrtownershipvisitor visit, void* context)
{ (void)value; (void)visit; (void)context; return true; }
static void blockedDrop(void* raw, void* context)
{
    (void)context; Gate* gate = raw; xrtownershipscope freeze = {0};
    /* A terminal child drop can inspect/join outside its enclosing Stack's
     * OWN mutation. It must never suspend some unknown caller's scope. */
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    ++gate->drops; xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE); await(&gate->proceed);
}
static const xvaluehandleops blockedOps = {NULL,blockedDrop,NULL,NULL};
static void workerBody(Gate* gate)
{
    if (gate->mode == 0) xl6StackClearV1(gate->stack,REF_ARGS);
    else if (gate->mode == 1) { xrtValueRelease(gate->stack); gate->stack = NULL; }
    else if (gate->mode == 2) assert(xl6StackTryTakeV1(gate->stack,REF_ARGS,0,&gate->target,true));
    else if (gate->mode == 3) assert(xl6StackTryTakeV1(gate->stack,REF_ARGS,0,&gate->target,false));
    else {
        xrtownershipscope refused = {0}; assert(!xrtOwnershipFreezeTryBegin(&refused));
        xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
        assert(xl6StackSizeV1(gate->stack,REF_ARGS,false) == 1);
    }
    assert(!xrtGetError()); xrtAtomic32Store(&gate->done,1,XMEMORY_RELEASE);
}
#ifdef _WIN32
static DWORD WINAPI worker(void* data) { workerBody(data); return 0; }
static void threaded(Gate* gate, HANDLE* thread)
{ *thread = CreateThread(NULL,0,worker,gate,0,NULL); assert(*thread); }
static void joined(HANDLE thread)
{ assert(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0 && CloseHandle(thread)); }
#else
static void* worker(void* data) { workerBody(data); return NULL; }
static void threaded(Gate* gate, pthread_t* thread) { assert(!pthread_create(thread,NULL,worker,gate)); }
static void joined(pthread_t thread) { assert(!pthread_join(thread,NULL)); }
#endif
static void callbacks(void)
{
    for (unsigned mode = 0; mode < 5; ++mode) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Gate gate = {.mode=mode};
        xrtAtomic32Init(&gate.entered,0); xrtAtomic32Init(&gate.proceed,0); xrtAtomic32Init(&gate.done,0);
        gate.stack = xl6StackCreateV1(REF_ARGS); assert(gate.stack);
        xvalue* weak = xrtValueWeakRef(gate.stack); assert(weak);
        void* raw = &gate; xvalue* child = xrtValueHandleTake(&raw,&blockedOps,NULL); assert(child);
        assert(xrtValueHandleOwnershipBindPhased(child,leafTrace));
        if (mode < 2) {
            assert(xl6StackPushV1(gate.stack,REF_ARGS,&child)); xrtValueRelease(child);
        } else {
            gate.target = child; child = xrtValueInt(42); assert(child);
            assert(xl6StackPushV1(gate.stack,REF_ARGS,&child)); xrtValueRelease(child);
        }
        xrtownershipscope freeze = {0};
        if (mode == 4) assert(xrtOwnershipFreezeTryBegin(&freeze));
#ifdef _WIN32
        HANDLE thread;
#else
        pthread_t thread;
#endif
        threaded(&gate,&thread); await(&gate.entered);
        assert(!xrtAtomic32Load(&gate.done,XMEMORY_ACQUIRE));
        if (mode == 4) assert(xrtOwnershipScopeEnd(&freeze));
        else {
            assert(xrtOwnershipFreezeTryBegin(&freeze));
            if (mode != 1) {
                xrtownershipsnapshot* snapshot = NULL; xrtownershipref root = xrtValueOwnership(gate.stack);
                assert(!xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,NULL,&snapshot) && !snapshot);
                xrtClearError(); /* BUSY storage is refused before tracing it. */
            }
            assert(xrtOwnershipScopeEnd(&freeze));
        }
        xrtAtomic32Store(&gate.proceed,1,XMEMORY_RELEASE); joined(thread);
        if (mode == 4) { /* This target was not used by the size query. */
            xrtValueRelease(gate.target); gate.target = NULL;
        }
        assert(gate.drops == 1); xrtValueRelease(gate.target); xrtValueRelease(gate.stack);
        assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);
    }
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc == 3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc == 1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); refusal(); callbacks(); balanced(&before);
    puts("stack ownership: real duplicate/self/mutual/shared-wrapper graphs, rooted abort and weak quarantine, opaque refusal, callback tails and cross-thread freeze; balanced");
    return 0;
}
