#include "src/runtime/xl6_foreign_resource.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static size_t drops;
static bool dispose(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)context;(void)result;void* raw=NULL;
    assert(frame->ArgumentCount==1 && xrtValueGetPointer(frame->Arguments[0],&raw));
    assert(raw && *(int*)raw==42);free(raw);++drops;return true;
}
static void* allocate(void) { int* raw=malloc(sizeof(*raw));assert(raw);*raw=42;return raw; }
static bool failDispose(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    assert(dispose(NULL,frame,result));
    if(context)xrtSetErrorKind(XERR_IO);
    return false;
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context;const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainForeignResourceV1Get();
    if(domain->Resolve(ref,NULL,&adapter,&preparation,&completion))return adapter;
    adapter=xl6ClosureOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtValueCallableOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtValueOwnershipAdapterV1(ref);
    return adapter;
}
static bool cycleDispose(void* data,const xrtcallframe* frame,xrtcallresult* result)
{
    XL6_Closure* closure=data;xvalue* captured=*(xvalue**)closure->environment;
    int64_t alive=0;
    assert(xrtValueGetInt(xrtValueObjectGet(captured,xrtStrView("alive")),&alive) && alive==42);
    return dispose(NULL,frame,result);
}
static bool resolveV3(xrtownershipref ref,void* context,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    *preparation=NULL;*completion=NULL;*adapter=resolve(ref,context);return *adapter!=NULL;
}
static void unused(void) { abort(); }
static void collectCycle(void)
{
    xvalue* root=xrtValueObject();assert(root);
    assert(xrtValueObjectSetNew(root,xrtStrView("alive"),xrtValueInt(42)));
    xvalue** environment=xrtMalloc(sizeof(*environment));assert(environment);
    *environment=xrtValueRetain(root);assert(*environment);
    const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
    const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(*environment),1,&slot};
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(NULL,unused,NULL,environment,&layout,NULL,NULL,1);assert(closure);
    xvalue* releaser=xl6ClosureBoxPointActionV1(NULL,closure,cycleDispose,UINT64_C(42));assert(releaser);
    xvalue* token=xl6ForeignResourceCreateV1(allocate(),releaser);assert(token);
    assert(xrtValueObjectSet(root,xrtStrView("resource"),token));
    xvalue* weak=xrtValueWeakRef(root);assert(weak);
    xrtValueRelease(token);xrtValueRelease(releaser);xl6ClosureRuntimeV1Get()->Release(closure);
    xrtValueRelease(root);size_t previous=drops;
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult ready=xl6OwnershipPlanTryCreateV3(xrtValueOwnership(root),resolveV3,NULL,&plan);
    assert(ready==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    XL6_OwnershipStepResult finalized=xl6OwnershipPlanFinalizeV2(plan);
    assert(finalized==XL6_OWNERSHIP_STEP_READY);
    assert(drops==previous+1);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak));xrtValueRelease(weak);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xrtparamdesc parameter={.Name={"raw",3},.Type=xrtTypePointer()};
    xrtfunctionsig signature={.Name={"dispose",7},.ParamCount=1,.Params=&parameter};
    xrtcallable* callable=xrtCallableCreate(&signature,dispose,NULL,NULL);assert(callable);
    xvalue* releaser=xrtValueCallableTake(&callable);assert(releaser && !callable);
    void* raw=allocate();xvalue* owner=xl6ForeignResourceCreateV1(raw,releaser);assert(owner);
    assert(xl6ForeignResourcePointerV1(owner)==raw);
    xvalue* alias=xrtValueRetain(owner);assert(alias);
    xrtValueRelease(owner);assert(drops==0);
    xrtValueRelease(alias);assert(drops==1 && !xrtGetError());
    for(unsigned order=0;order<4;++order){
        raw=allocate();size_t previous=drops;
        owner=xl6ForeignResourceCreateV1(raw,releaser);assert(owner);
        xvalue* copy=order<2 ? xrtValueClone(owner) : xrtValueDeepClone(owner);assert(copy);
        xvalue* survivor=(order&1) ? owner : copy;
        xrtValueRelease((order&1) ? copy : owner);assert(drops==previous);
        assert(xl6ForeignResourcePointerV1(survivor)==raw);
        xrtValueRelease(survivor);assert(drops==previous+1 && !xrtGetError());
    }
    xmemdebugsnapshot before={0},after={0};xrtMemDebugSnapshot(&before);
    size_t failed=0,passed=0;
    for(int64_t fault=0;fault<32;++fault){
        raw=allocate();size_t previous=drops;
        assert(xrtMemDebugFailAfter(fault));
        owner=xl6ForeignResourceCreateV1(raw,releaser);
        xrtMemDebugFailClear();xrtClearError();
        if(owner){assert(xl6ForeignResourcePointerV1(owner)==raw);xrtValueRelease(owner);assert(drops==previous+1);++passed;}
        else { assert(drops==previous);free(raw);++failed; }
        xrtClearError();xrtMemDebugSnapshot(&after);assert(after.LiveCount==before.LiveCount);
    }
    assert(failed && passed);
    raw=allocate();assert(!xl6ForeignResourceCreateV1(raw,xrtValueNull()));free(raw);xrtClearError();
    for(unsigned bad=0;bad<6;++bad){
        xrtparamdesc argument=parameter;
        const xrttype* returns[]={xrtTypeInt64()};
        xrtfunctionsig invalid=signature;
        invalid.Params=&argument;
        if(bad==0){invalid.ParamCount=0;invalid.Params=NULL;}
        if(bad==1)argument.Type=xrtTypeInt64();
        if(bad==2){invalid.ReturnCount=1;invalid.ReturnTypes=returns;}
        if(bad==3)argument.Mode=XRT_PARAM_BYREF;
        if(bad==4)invalid.Flags=XRT_FUNCTION_FLAG_VARARGS;
        callable=xrtCallableCreate(bad==5 ? NULL : &invalid,dispose,NULL,NULL);assert(callable);
        xvalue* wrong=xrtValueCallableTake(&callable);assert(wrong);
        raw=allocate();size_t previous=drops;
        assert(!xl6ForeignResourceCreateV1(raw,wrong) && drops==previous);
        xrtClearError();free(raw);xrtValueRelease(wrong);
    }
    for(unsigned diagnostic=0;diagnostic<3;++diagnostic){
        callable=xrtCallableCreate(&signature,failDispose,diagnostic==1 ? &drops : NULL,NULL);assert(callable);
        xvalue* failing=xrtValueCallableTake(&callable);assert(failing);
        owner=xl6ForeignResourceCreateV1(allocate(),failing);assert(owner);
        size_t previous=drops;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        if(diagnostic==2)assert(xrtMemDebugFailAfter(0));
        xrtValueRelease(owner);
        xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && drops==previous+1);
        assert(diagnostic==2 ? !outcome.error || xrtErrorKind(outcome.error)==XERR_MEMORY :
            xrtErrorKind(outcome.error)==(diagnostic ? XERR_IO : XERR_STATE));
        xrtErrorFree(outcome.error);xrtValueRelease(failing);xrtClearError();
    }
    xrtMemDebugSnapshot(&before);collectCycle();xrtClearError();
    xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount);
    xrtValueRelease(releaser);
    printf("foreign resource faults: %zu refused, %zu completed\n",failed,passed);
    return 0;
}
