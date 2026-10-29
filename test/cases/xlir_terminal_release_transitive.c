#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_STRING
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

/* Real ordinary ownership, not a plan-held receiver or a test protection
 * lease. A weak reference only keeps the terminal shell's address readable. */
static xvalue *receiver, *weak;
static xvalueiter* kept;
static bool expectIsolation=true, keepCursor, failFinalizer;
static unsigned finalized, isolated, probes, opaqueDrops;
static DWORD WINAPI inspect(void* unused)
{
    xrtownershipscope freeze={0}; DWORD begin=GetTickCount(); (void)unused;
    do {
        if(xrtOwnershipFreezeTryBegin(&freeze)) {
            if(receiver) {
                size_t n=181; xrtownershipref ref=xrtValueOwnership(receiver);
                assert(!ref.Ops->Count(ref.Data,&n) && n==181);
            }
            assert(xrtOwnershipScopeEnd(&freeze)); return 0;
        }
        SwitchToThread();
    } while((DWORD)(GetTickCount()-begin)<250);
    return 1;
}
static void probe(void)
{
    DWORD result=99; HANDLE thread=CreateThread(NULL,0,inspect,NULL,0,NULL);
    assert(thread && WaitForSingleObject(thread,5000)==WAIT_OBJECT_0 && GetExitCodeThread(thread,&result) && CloseHandle(thread));
    ++probes; if(!result)++isolated;
    assert((result==0)==expectIsolation);
}
static bool finalize(xvalue* value,void* unused)
{
    int64 field=0; (void)unused;
    assert(value==receiver && xrtValueWeakRefExpired(weak));
    assert(xrtValueType(xrtValueWeakRefLock(weak))==XVALUE_NULL);
    assert(xrtValueGetInt(xrtValueObjectGet(value,xrtStrView("field")),&field) && field==181);
    assert(xrtValueObjectSetNew(value,xrtStrView("field"),xrtValueInt(182)));
    ++finalized; probe();
    if(keepCursor) { assert(!kept); kept=xrtValueIterCreate(value); assert(kept); }
    if(failFinalizer) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
static uint64 identity_hash(const xvalue* value,void* unused)
{ (void)value; (void)unused; return 181; }
static bool identity_equal(const xvalue* a,const xvalue* b,void* unused)
{ (void)unused; return a==b; }
static xvalue* object(void)
{
    xvalue* value=xl6ClassRuntimeV2Get()->Create(NULL,finalize); assert(value);
    assert(xrtValueTypeIdBind(value,181) && xl6ClassRuntimeV2Get()->BindIdentity(value,identity_hash,identity_equal));
    assert(xrtValueObjectSetNew(value,xrtStrView("field"),xrtValueInt(181)));
    value=xl6ClassRuntimeV2Get()->CommitTake(value); assert(value);
    receiver=value; weak=xrtValueWeakRef(value); assert(weak); return value;
}
static void entry(void) {}
static bool box_entry(void* data,const xrtcallframe* frame,xrtcallresult* result)
{ (void)data; (void)frame; (void)result; assert(false); return false; }
static const XL6_ClosureCaptureSlot valueSlot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
static const XL6_ClosureEnvironmentV1 valueLayout={sizeof(valueLayout),sizeof(xvalue*),1,&valueSlot};
static XL6_Closure* closure(xvalue* value)
{
    xvalue** environment=xrtMalloc(sizeof(*environment)); assert(environment); *environment=value;
    XL6_Closure* result=xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,environment,&valueLayout,NULL,NULL,0);
    assert(result); return result;
}
static xfuture* future(void* owner,XL6_FuturePayloadKind kind)
{
    xfuture* result=NULL; xpromise* promise=xrtPromiseCreate(&result,NULL); assert(promise && result);
    assert(xl6PayloadResolveOwned(promise,owner,kind)); xrtPromiseDestroy(promise); return result;
}
static xfuture* value_future(xvalue* value)
{
    xvalue** box=xrtMalloc(sizeof(*box)); assert(box); *box=value;
    return future(box,XL6_FUTURE_PAYLOAD_XVALUE);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void run(unsigned mode)
{
    xmemdebugsnapshot before; unsigned calls=finalized; xrtMemDebugSnapshot(&before);
    keepCursor=mode==18; failFinalizer=mode>=19;
    xvalue* value=object(); const xerror* prior=NULL; xl6_error_scope cleanup={0};
    if(mode==20) {
        /* A failed LANGUAGE cleanup has an explicit first-error boundary.
         * A bare native diagnostic is not an already escaping language error. */
        xrtSetErrorKind(XERR_ARGUMENT); prior=xrtGetError(); assert(prior);
        assert(xl6ErrorScopeBeginCleanup(&cleanup,false));
    }
    switch(mode) {
    case 0: case 15: case 16: {
        xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendTake(array,&value) && !value);
        if(mode==15) {
            xvalueiter* iterator=xrtValueIterCreate(array); assert(iterator); xrtValueRelease(array);
            assert(finalized==calls); xrtValueIterDestroy(iterator);
        } else if(mode==16) {
            xvalue* copy=xrtValueClone(array); assert(copy && copy!=array); xrtValueRelease(array);
            assert(finalized==calls); xrtValueRelease(copy);
        } else xrtValueRelease(array);
        break;
    }
    case 1: {
        xvalue* map=xrtValueIntMap(); assert(map && xrtValueIntMapSetTake(map,7,&value) && !value);
        xrtValueRelease(map); break;
    }
    case 2: {
        xvalue* set=xrtValueSet(); assert(set && xrtValueSetAddTake(set,&value) && !value);
        xrtValueRelease(set); break;
    }
    case 3: case 4: {
        xvalue* parent=xrtValueObject(); assert(parent && xrtValueObjectSetTake(parent,xrtStrView("child"),&value));
        if(mode==4) { xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendTake(array,&parent)); parent=array; }
        xrtValueRelease(parent); break;
    }
    case 5: case 6: {
        XL6_Cell* cell=xl6CellRuntimeV1Get()->Create(XL6_CELL_XVALUE,sizeof(value)); assert(cell);
        xl6CellRuntimeV1Get()->Initialize(cell,&value,sizeof(value));
        if(mode==5) xl6CellRuntimeV1Get()->Release(cell);
        else {
            static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_CELL,0,sizeof(XL6_Cell*)};
            static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(XL6_Cell*),1,&slot};
            XL6_Cell** environment=xrtMalloc(sizeof(*environment)); assert(environment); *environment=cell;
            XL6_Closure* c=xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,environment,&layout,NULL,NULL,0); assert(c);
            xl6ClosureRuntimeV1Get()->Release(c);
        }
        break;
    }
    case 7: {
        XL6_Closure* c=closure(value); XL6_ClosureScope scope={0};
        xl6ClosureRuntimeV1Get()->Enter(&scope,c); xl6ClosureRuntimeV1Get()->Release(c);
        assert(finalized==calls); xl6ClosureRuntimeV1Get()->Leave(&scope); break;
    }
    case 8: xrtFutureDestroy(value_future(value)); break;
    case 9: {
        XL6_Closure** box=xrtMalloc(sizeof(*box)); assert(box); *box=closure(value);
        xrtFutureDestroy(future(box,XL6_FUTURE_PAYLOAD_CLOSURE)); break;
    }
    case 10: case 11: {
        xfuture* f=value_future(value); xvalue* wrapper=xrtValueFutureTake(&f); assert(wrapper && !f);
        if(mode==11) { xvalue* copy=xrtValueDeepClone(wrapper); assert(copy && copy!=wrapper);
            xrtValueRelease(wrapper); assert(finalized==calls); wrapper=copy; }
        xrtValueRelease(wrapper); break;
    }
    case 12: case 13: {
        XL6_Closure* c=closure(value); xvalue* wrapper=xl6ClosureRuntimeV1Get()->Box(NULL,c,box_entry,181); assert(wrapper);
        xl6ClosureRuntimeV1Get()->Release(c);
        if(mode==13) { xvalue* copy=xrtValueDeepClone(wrapper); assert(copy && copy!=wrapper);
            xrtValueRelease(wrapper); assert(finalized==calls); wrapper=copy; }
        xrtValueRelease(wrapper); break;
    }
    case 14: {
        xvalueiter* iterator=xrtValueIterCreate(value); assert(iterator); xrtValueRelease(value);
        assert(finalized==calls); xrtValueIterDestroy(iterator); break;
    }
    case 17: {
        xvalue** environment=xrtMalloc(sizeof(*environment)); assert(environment); *environment=value;
        xl6ClosureRuntimeV2Get()->DropEnvironment(&valueLayout,environment); break;
    }
    default: xrtValueRelease(value); break;
    }
    assert(finalized==calls+1 && xrtValueWeakRefExpired(weak));
    if(mode==20) {
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        assert(!outcome.ok && outcome.error==prior && xrtGetError()==prior);
        xrtErrorFree(outcome.error);
    }
    else if(mode==19) assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE);
    else assert(!xrtGetError());
    xrtClearError();
    if(kept) { xvalueiter* iterator=kept; kept=NULL; xrtValueIterDestroy(iterator); }
    xrtValueRelease(weak); weak=receiver=NULL; balanced(&before);
}
static bool native_trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{ (void)value; (void)visit; (void)context; assert(false); return false; }
static bool native_clone(ptr handle,ptr* result,ptr context)
{ (void)handle; (void)result; (void)context; return false; }
static void native_drop(ptr handle,ptr context)
{ assert(!context); ++opaqueDrops; probe(); xrtFree(handle); }
static const xvaluehandleops nativeOps={native_clone,native_drop,NULL,NULL};
int main(void)
{
    xmemdebugsnapshot before; assert(xrtMemDebugEnable(true)); xrtSetErrorKind(XERR_STATE); xrtClearError();
    for(unsigned mode=0;mode<21;++mode) for(unsigned i=0;i<25;++i) run(mode);
    assert(finalized==525 && probes==525 && isolated==525);
    /* An enclosing caller scope is never silently suspended, and tracing is
     * not an implicit native phase/collection certificate. */
    xrtMemDebugSnapshot(&before); expectIsolation=false; keepCursor=failFinalizer=false;
    xvalue* value=object(); xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
    xrtValueRelease(value); assert(xrtOwnershipScopeEnd(&mutation));
    xrtValueRelease(weak); weak=receiver=NULL; balanced(&before);
    for(unsigned phased=0;phased<2;++phased) {
        xrtMemDebugSnapshot(&before); void* handle=xrtMalloc(1); assert(handle);
        xvalue* wrapper=xrtValueHandleTake(&handle,&nativeOps,NULL); assert(wrapper && !handle);
        assert(phased?xrtValueHandleOwnershipBindPhased(wrapper,native_trace):xrtValueHandleOwnershipBind(wrapper,native_trace));
        xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(!xrtValueOwnershipAdapterV1(xrtValueOwnership(wrapper))); assert(xrtOwnershipScopeEnd(&freeze));
        expectIsolation=phased!=0; xrtValueRelease(wrapper); balanced(&before);
    }
    assert(finalized==526 && probes==528 && isolated==526 && opaqueDrops==2);
    puts("Terminal release isolation: 21 routes x25, 526 actual finalizers, 528 cross-thread probes; containers/COW/cursors/cells/closure scopes/Future/callable bridges/deep clones, receiver/weak/error contracts, explicit native phases and caller-scope preservation; zero live delta");
    return 0;
}
