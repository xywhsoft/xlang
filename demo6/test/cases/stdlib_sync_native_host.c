#include "src/runtime/xl6_sync.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_sync_admission.h"
#include "src/runtime/xl6_array.h"
#include "release/include/xrt_decl.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { MUTEX_ID=501,COND_ID=502,RWLOCK_ID=503 };
static bool collect(xrtownershipref ref,void* context)
{ *(xrtownershipref*)context=ref; return true; }
static xrtownershipref ownerRef(xvalue* value)
{
    xrtownershipref result={0},shell=xrtValueOwnership(value);
    assert(shell.Data && shell.Ops->Trace(shell.Data,collect,&result));
    assert(result.Data && xl6SyncOwnershipAdapterV1(result)); return result;
}
static void fakeDrop(void* data,void* context) { (void)data; (void)context; }
static void vectors(void)
{
    size_t total=99;
    assert(!xlSyncRetirementSnapshotCheckedV1(NULL,1,&total) && !total); xrtClearError();
    assert(!xlSyncRetirementAcquireCheckedV1(0,NULL)); xrtClearError();
    assert(!xlSyncRetirementUnlockCheckedV1(NULL,1)); xrtClearError();
    assert(!xl6MutexCreateV1(0)); xrtClearError();
    xvalue* mutex=xl6MutexCreateV1(MUTEX_ID); xvalue* cond=xl6CondCreateV1(COND_ID);
    xvalue* rw=xl6RWLockCreateV1(RWLOCK_ID); assert(mutex && cond && rw);
    assert(xl6SyncAdmitsV1(mutex,MUTEX_ID,1) && xl6SyncAdmitsV1(cond,COND_ID,2) && xl6SyncAdmitsV1(rw,RWLOCK_ID,3));
    assert(!xl6SyncAdmitsV1(mutex,MUTEX_ID,2) && !xl6SyncAdmitsV1(mutex,MUTEX_ID+1,1));
    static const xvaluehandleops fakeOps={NULL,fakeDrop,NULL,NULL}; void* bad=(void*)(uintptr_t)1;
    xvalue* fake=xrtValueHandleTake(&bad,&fakeOps,NULL); assert(fake && xrtValueTypeIdBind(fake,MUTEX_ID));
    assert(!xl6SyncAdmitsV1(fake,MUTEX_ID,1) && !xl6SyncOwnershipAdapterV1(xrtValueOwnership(fake)));
    xvalue* alias=xrtValueClone(mutex); assert(alias);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(xl6MutexLockV1(alias,MUTEX_ID)); assert(!xl6MutexTryLockV1(mutex,MUTEX_ID));
    assert(!xl6CondWaitTimeoutV1(cond,COND_ID,mutex,MUTEX_ID,-1));
    assert(xl6MutexUnlockV1(mutex,MUTEX_ID) && !xl6MutexUnlockV1(alias,MUTEX_ID));
    assert(xl6CondSignalV1(cond,COND_ID) && xl6CondBroadcastV1(cond,COND_ID));
    assert(xl6RWLockReadV1(rw,RWLOCK_ID) && xl6RWLockTryReadV1(rw,RWLOCK_ID));
    assert(!xl6RWLockTryWriteV1(rw,RWLOCK_ID));
    assert(xl6RWLockReadUnlockV1(rw,RWLOCK_ID) && xl6RWLockReadUnlockV1(rw,RWLOCK_ID));
    assert(xl6RWLockTryWriteV1(rw,RWLOCK_ID) && xl6RWLockDowngradeV1(rw,RWLOCK_ID));
    assert(xl6RWLockUpgradeV1(rw,RWLOCK_ID) && xl6RWLockWriteUnlockV1(rw,RWLOCK_ID));
    assert(xrtGetError()==prior);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SyncAdmitsV1(mutex,MUTEX_ID,1) && !xl6SyncRetirementSnapshotV1(NULL,0));
    assert(!xrtMemDebugFailTriggered() && xrtGetError()==prior); xrtMemDebugFailClear(); xrtClearError();
    xrtValueRelease(fake); xrtValueRelease(alias); xrtValueRelease(mutex); xrtValueRelease(cond); xrtValueRelease(rw);
}
static void nullPrimary(void)
{
    /* Explicit inherited failure-bit contract, not a claim that native OOM
     * has been observed without its static diagnostic. */
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    xl6ErrorScopeCaptureFailure(&outer); assert(!xrtGetError());
    size_t total=99;
    assert(xlSyncRetirementSnapshotCheckedV1(NULL,0,&total) && !total);
    xvalue* value=NULL; assert(!xlSyncRetirementAcquireCheckedV1(0,&value) && !value && !xrtGetError());
    assert(!xlSyncRetirementRetryCheckedV1(0) && !xrtGetError());
    xl6_error_result result=xl6ErrorScopeEnd(&outer); assert(!result.ok && !result.error && !xrtGetError());
}
/* The language shell really expires. Failed cleanup retains a native owner,
 * not that dead shell or a fake graph root. Native Unit must keep refusing
 * until a genuine recovery handle explicitly unlocks the original object. */
static void heldDrop(unsigned kind,bool write)
{
    uint64_t id=kind==1 ? MUTEX_ID : RWLOCK_ID;
    xvalue* value=kind==1 ? xl6MutexCreateV1(id) : xl6RWLockCreateV1(id); assert(value);
    assert(kind==1 ? xl6MutexLockV1(value,id) : write ? xl6RWLockWriteV1(value,id) : xl6RWLockReadV1(value,id));
    xvalue* weak=xrtValueWeakRef(value); assert(weak);
    xl6_error_scope cleanup={0}; assert(xl6ErrorScopeBeginCleanup(&cleanup,false));
    xrtValueRelease(value);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    assert(!result.ok && result.error && xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    assert(!xrtGetError() && xrtValueWeakRefExpired(weak));
    XL6_SyncRetirementV1 row;
    assert(xl6SyncRetirementSnapshotV1(&row,1)==1 && row.token && row.identity==id && row.kind==kind && row.errorKind==XERR_STATE);
    uint64_t token=row.token;
    assert(!xlSyncRetirementRetryCheckedV1(token)); assert(xrtGetError()); xrtClearError();
    assert(xl6SyncRetirementSnapshotV1(&row,1)==1 && row.token==token);
    for(size_t point=0;point<16;++point) {
        assert(xrtMemDebugFailAfter(point));
        xvalue* repair=NULL; int acquired=xlSyncRetirementAcquireCheckedV1(token,&repair);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) { assert(!repair && !acquired); xrtClearError(); }
        else { assert(acquired && repair && !xrtGetError()); xrtValueRelease(repair); break; }
        assert(point<15 && xl6SyncRetirementSnapshotV1(&row,1)==1 && row.token==token);
    }
    xvalue* repair=NULL; assert(xlSyncRetirementAcquireCheckedV1(token,&repair) && repair);
    assert(!xl6SyncAdmitsV1(repair,id,kind));
    assert(kind==1 ? !xl6MutexTryLockV1(repair,id) : !xl6RWLockTryReadV1(repair,id));
    unsigned int mode=kind==1 ? XL_SYNC_UNLOCK_MUTEX_V1 : write ? XL_SYNC_UNLOCK_WRITE_V1 : XL_SYNC_UNLOCK_READ_V1;
    assert(!xlSyncRetirementUnlockCheckedV1(repair,99)); xrtClearError();
    assert(xlSyncRetirementUnlockCheckedV1(repair,mode));
    assert(xlSyncRetirementRetryCheckedV1(token) && !xl6SyncRetirementSnapshotV1(NULL,0));
    assert(kind==1 ? !xl6MutexUnlockV1(repair,id) : !xl6RWLockReadUnlockV1(repair,id));
    xrtValueRelease(repair); xrtValueRelease(weak);
    assert(!xl6SyncRetirementAcquireV1(token)); xrtClearError();
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context;
    const XL6_SequenceAdmissionOpsV2* policies[]={xl6ArraySyncAdmissionOpsV2Get()};
    const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV2(ref,policies,1);
    if (!adapter) adapter=xl6SyncOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static bool resolveV2(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation)
{
    *preparation=NULL; *adapter=resolve(ref,context); return *adapter!=NULL;
}
static void realPlanHeld(bool inArray)
{
    xvalue* mutex=xl6MutexCreateV1(MUTEX_ID); assert(mutex && xl6MutexLockV1(mutex,MUTEX_ID));
    xvalue* root=mutex;
    if (inArray) {
        root=xl6SequenceSyncCreateV1(1,601,MUTEX_ID,MUTEX_ID,false,1); assert(root);
        assert(xl6ArrayPushV1(root,601,MUTEX_ID,3,sizeof(xvalue*),xrtTypeValue(),&mutex));
        xrtValueRelease(mutex);
    }
    xvalue* weak=xrtValueWeakRef(root); assert(weak);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(xrtValueOwnership(root),resolveV2,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
    xrtValueRelease(root); /* The plan, not an invented root, now owns the actual nodes. */
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_FAILED && xrtGetError()==prior);
    xrtClearError();
    XL_SyncRetirementV1 row; size_t total=0;
    assert(xlSyncRetirementSnapshotCheckedV1(&row,1,&total) && total==1 && row.kind==1 && row.errorKind==XERR_STATE);
    xvalue* repair=NULL; assert(xlSyncRetirementAcquireCheckedV1(row.token,&repair));
    assert(xlSyncRetirementUnlockCheckedV1(repair,XL_SYNC_UNLOCK_MUTEX_V1));
    assert(xlSyncRetirementRetryCheckedV1(row.token)); xrtValueRelease(repair);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    assert(xlSyncRetirementSnapshotCheckedV1(NULL,0,&total) && !total && !xrtGetError());
    puts("sync genuine ownership plan: failed Finish retains actual shell/owner pins, host repair, idempotent retry balanced");
}
static void policies(void)
{
    for (unsigned kind=1;kind<=3;++kind) {
        uint64_t id=900+kind;
        xvalue* object=kind==1 ? xl6MutexCreateV1(id) : kind==2 ? xl6CondCreateV1(id) : xl6RWLockCreateV1(id);
        xvalue* a=xl6SequenceSyncCreateV1(1,800+kind,id,id,false,kind);
        xvalue* b=xl6SequenceSyncCreateV1(2,810+kind,id,id,false,kind);
        xvalue* optional=xl6SequenceSyncCreateV1(1,820+kind,id+10,id,true,kind);
        xvalue* foreignKind=xl6SequenceSyncCreateV1(1,830+kind,id,id,false,kind==3 ? 1 : kind+1);
        assert(object && a && b && optional && foreignKind);
        XL6_SequenceRoleV1 role; XL6_SequenceLayoutV1 layout;
        XL6_SequenceAdmissionV3 x={0},y={0},z={0},w={0};
        assert(xl6SequenceSchemaCopyV1(a,&role,&layout,&x) && xl6SequenceSchemaCopyV1(b,&role,&layout,&y));
        assert(xl6SequenceSchemaCopyV1(optional,&role,&layout,&z) && xl6SequenceSchemaCopyV1(foreignKind,&role,&layout,&w));
        assert(x.ops==xl6ArraySyncAdmissionOpsV2Get() && x.data!=y.data && x.ops->Equivalent(x.data,y.data));
        assert(!x.ops->Equivalent(x.data,z.data) && !x.ops->Equivalent(x.data,w.data));
        assert(!x.ops->Defaultable(x.data) && z.ops->Defaultable(z.data));
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
        assert(xrtMemDebugFailAfter(0));
        assert(x.ops->Value(x.data,object) && !w.ops->Value(w.data,object));
        assert(!x.ops->Value(x.data,xrtValueNull()) && z.ops->Value(z.data,xrtValueNull()));
        assert(!xrtMemDebugFailTriggered() && xrtGetError()==prior); xrtMemDebugFailClear(); xrtClearError();
        xl6SequenceAdmissionDropV3(&x); xl6SequenceAdmissionDropV3(&y); xl6SequenceAdmissionDropV3(&z); xl6SequenceAdmissionDropV3(&w);
        xrtValueRelease(object); xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(optional); xrtValueRelease(foreignKind);
    }
    size_t point;
    for (point=0;point<64;++point) {
        assert(xrtMemDebugFailAfter(point));
        xvalue* value=xl6SequenceSyncCreateV1(1,601,MUTEX_ID,MUTEX_ID,false,1);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(hit ? value==NULL : value!=NULL); xrtClearError(); xrtValueRelease(value);
        if (!hit) break;
    }
    assert(point && point<64); printf("sync owned leaf policy: complete allocation prefix %zu\n",point);
}
static void phasedHeld(void)
{
    xvalue* mutex=xl6MutexCreateV1(MUTEX_ID); assert(mutex && xl6MutexLockV1(mutex,MUTEX_ID));
    xrtownershipref ref=ownerRef(mutex); const xrtownershipadapterv1* adapter=xl6SyncOwnershipAdapterV1(ref); assert(adapter);
    int token;
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token)); adapter->Clear(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(!adapter->Finish(ref.Data,&token) && xrtGetError()==prior); xrtClearError();
    XL6_SyncRetirementV1 row; assert(xl6SyncRetirementSnapshotV1(&row,1)==1);
    xvalue* repair=xl6SyncRetirementAcquireV1(row.token); assert(repair && xl6MutexUnlockV1(repair,MUTEX_ID));
    xrtValueRelease(repair);
    assert(adapter->Finish(ref.Data,&token) && adapter->Finish(ref.Data,&token));
    assert(!xl6SyncRetirementSnapshotV1(NULL,0));
    adapter->Drop(ref.Data); xrtValueRelease(mutex); assert(!xrtGetError());
}
typedef struct CondWaiter { const xvalue* cond; const xvalue* mutex; } CondWaiter;
static int32 waitWorker(void* raw)
{
    const CondWaiter* work=raw; assert(xl6MutexLockV1(work->mutex,MUTEX_ID));
    assert(!xl6CondWaitTimeoutV1(work->cond,COND_ID,work->mutex,MUTEX_ID,500));
    assert(xl6MutexUnlockV1(work->mutex,MUTEX_ID)); assert(!xrtGetError()); return 0;
}
static void activeWait(void)
{
    xvalue* cond=xl6CondCreateV1(COND_ID); xvalue* mutex=xl6MutexCreateV1(MUTEX_ID); assert(cond && mutex);
    xrtownershipref ref=ownerRef(cond); xvalue* weak=xrtValueWeakRef(cond); assert(weak);
    CondWaiter work={cond,mutex}; xthread* thread=xrtThreadCreate(waitWorker,&work,0); assert(thread);
    bool active=false;
    for(unsigned i=0;i<10000 && !active;++i) {
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)) {
            active=!xl6SyncOwnershipAdapterV1(ref); assert(xrtOwnershipScopeEnd(&freeze));
        }
        if(!active) xrtThreadYield();
    }
    assert(active && xl6SyncAdmitsV1(cond,COND_ID,2));
    xrtValueRelease(cond); assert(xrtValueWeakRefExpired(weak));
    assert(xrtThreadWaitFor(thread,UINT64_C(30000000))==XWAIT_OK); xrtThreadDestroy(thread);
    xrtValueRelease(weak); xrtValueRelease(mutex); assert(!xl6SyncRetirementSnapshotV1(NULL,0));
}
static void allocations(unsigned kind)
{
    size_t point;
    for(point=0;point<64;++point) {
        assert(xrtMemDebugFailAfter(point));
        xvalue* value=kind==1 ? xl6MutexCreateV1(MUTEX_ID) : kind==2 ? xl6CondCreateV1(COND_ID) : xl6RWLockCreateV1(RWLOCK_ID);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) assert(!value); else assert(value && !xrtGetError());
        xrtClearError(); xrtValueRelease(value); assert(!xl6SyncRetirementSnapshotV1(NULL,0));
        if(!hit) break;
    }
    assert(point>0 && point<64); printf("sync family %u: complete allocation prefix %zu\n",kind,point);
}
typedef struct Worker { const xvalue* mutex; uint64_t token; } Worker;
static int32 concurrentWorker(void* raw)
{
    const Worker* work=raw;
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    for (unsigned i=0;i<1000;++i) {
        if (work->token) {
            XL_SyncRetirementV1 row; size_t total=0;
            assert(xlSyncRetirementSnapshotCheckedV1(&row,1,&total) && total==1 && row.token==work->token);
            xvalue* repair=NULL;
            int acquired=xlSyncRetirementAcquireCheckedV1(work->token,&repair);
            /* Concurrent Retry may temporarily reserve physical Unit. It is
             * legitimate to refuse Acquire, but never return an unsafe shell. */
            assert(acquired ? repair!=NULL : repair==NULL);
            if (repair) assert(!xlSyncRetirementUnlockCheckedV1(repair,XL_SYNC_UNLOCK_MUTEX_V1));
            assert(!xlSyncRetirementRetryCheckedV1(work->token));
            xrtValueRelease(repair);
        } else {
            assert(xl6MutexLockV1(work->mutex,MUTEX_ID));
            assert(xl6MutexUnlockV1(work->mutex,MUTEX_ID));
        }
        assert(xrtGetError()==prior);
    }
    xrtClearError(); return 0;
}
static void concurrentActivity(void)
{
    xvalue* mutex=xl6MutexCreateV1(MUTEX_ID); assert(mutex);
    for (unsigned phase=0;phase<2;++phase) {
        Worker work={mutex,0};
        if (phase) {
            assert(xl6MutexLockV1(mutex,MUTEX_ID));
            xrtValueRelease(mutex); mutex=NULL; work.mutex=NULL; assert(xrtGetError()); xrtClearError();
            XL_SyncRetirementV1 row; size_t total=0;
            assert(xlSyncRetirementSnapshotCheckedV1(&row,1,&total) && total==1); work.token=row.token;
        }
        xthread* threads[4];
        for (unsigned i=0;i<4;++i) { threads[i]=xrtThreadCreate(concurrentWorker,&work,0); assert(threads[i]); }
        for (unsigned i=0;i<4;++i) {
            assert(xrtThreadWaitFor(threads[i],UINT64_C(30000000))==XWAIT_OK); xrtThreadDestroy(threads[i]);
        }
        if (phase) {
            xvalue* repair=NULL; assert(xlSyncRetirementAcquireCheckedV1(work.token,&repair));
            assert(xlSyncRetirementUnlockCheckedV1(repair,XL_SYNC_UNLOCK_MUTEX_V1));
            assert(xlSyncRetirementRetryCheckedV1(work.token)); xrtValueRelease(repair);
        } else {
            xrtownershipref owner=ownerRef(mutex); size_t refs=0;
            xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
            assert(owner.Ops->Count(owner.Data,&refs) && refs==1); assert(xrtOwnershipScopeEnd(&freeze));
        }
    }
    assert(!xl6SyncRetirementSnapshotV1(NULL,0));
    puts("sync concurrent activity/retirement: real calls, concurrent snapshots/acquisitions/checked wrong-thread unlock/retry, no lost pins");
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==2 && !strcmp(argv[1],"--race-stress-only")) concurrentActivity();
    else {
        assert(argc==1);
        vectors(); nullPrimary(); policies(); heldDrop(1,false); heldDrop(3,false); heldDrop(3,true); phasedHeld();
        realPlanHeld(false); realPlanHeld(true); activeWait();
        concurrentActivity(); allocations(1); allocations(2); allocations(3);
    }
    assert(!xrtGetError() && !xl6SyncRetirementSnapshotV1(NULL,0) && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("sync resident core: genuine families, actual expired shells/held native owners, explicit recovery/retry, checked failures, active wait and OOM balanced");
    return 0;
}
