/* Actual native Drop finalization, rather than direct mailbox injection. */
#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_callback_failure.h"
#include "src/runtime/xl6_native_callback.h"
#include <assert.h>
#include <stdio.h>

typedef struct DropEnvironment {xerror* failure;} DropEnvironment;
typedef struct DropWork {XL6_NativeCallback* record;xerror* diagnostic;bool oom;} DropWork;
static const int layoutIdentity=1;
static unsigned finalizers;
static void invoke(void){}
static bool valid(const void* layout,const void* environment,size_t bytes)
{return layout==&layoutIdentity && environment && bytes==sizeof(DropEnvironment);}
static bool trace(const void* layout,const void* environment,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{
    assert(valid(layout,environment,bytes));
    const DropEnvironment* value=environment;
    return !value->failure || visit(xrtErrorOwnership(value->failure),user);
}
static bool drop(const void* layout,void* environment,size_t bytes)
{
    assert(valid(layout,environment,bytes));
    DropEnvironment* value=environment;xerror* failure=value->failure;
    xrtFree(value);++finalizers;xrtSetErrorTake(failure);return false;
}
static const XL6_ClosureEnvironmentOpsV1 policy={sizeof(policy),valid,trace,drop};
static XL6_NativeCallback* create(xerror* failure)
{
    DropEnvironment* environment=xrtMalloc(sizeof(*environment));assert(environment);environment->failure=failure;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,invoke,NULL,environment,sizeof(*environment),
        &layoutIdentity,&policy,NULL,NULL,0);assert(closure);
    XL6_NativeCallback* record=xl6NativeCallbackRuntimeV1Get()->Create(NULL,closure,NULL,1);assert(record);
    xl6ClosureRuntimeV1Get()->Release(closure);return record;
}
static int32 worker(void* raw)
{
    DropWork* work=raw;XL6_ForeignCallbackScopeV1 warm={0};
    assert(xl6ForeignCallbackEnterV1(&warm) && xl6ForeignCallbackLeaveV1(&warm));
    if(work->oom)assert(xrtMemDebugFailAfter(0));
    assert(!xl6NativeCallbackRuntimeV1Get()->DropNative(work->record));
    work->diagnostic=xrtTakeError();
    if(work->oom){assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
    return 0;
}
static void count(uint64_t expectedPending,uint64_t expectedTotal)
{uint64_t pending,total;assert(xl6CallbackFailureCountV1(&pending,&total));assert(pending==expectedPending && total==expectedTotal);}
static xl6_error_result boundary(void)
{xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));return xl6ErrorScopeEndAtSafePointV1(&scope);}
int main(void)
{
    assert(boundary().ok);
    for(unsigned diagnostic=0;diagnostic<2;++diagnostic){
        xerror* error=diagnostic ? xrtErrorCreate(XERR_STATE,"native drop",7,"owned environment failed") : NULL;
        assert(!diagnostic || error);
        DropWork work={create(error),NULL,!diagnostic};
        xthread* thread=xrtThreadCreate(worker,&work,0);assert(thread);
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
        assert(work.diagnostic==error && finalizers==diagnostic+1);count(1,diagnostic+1);
        /* Native C and the pending language channel own independent references. */
        xrtErrorFree(work.diagnostic);
        xl6_error_scope failed={0};assert(xl6ErrorScopeBegin(&failed));xl6ErrorScopeCaptureFailure(&failed);
        xl6_error_result primary=xl6ErrorScopeEndAtSafePointV1(&failed);
        assert(!primary.ok && !primary.error);count(1,diagnostic+1);
        xl6_error_result delivered=boundary();assert(!delivered.ok && delivered.error==error);
        xrtErrorFree(delivered.error);count(0,diagnostic+1);assert(boundary().ok);
    }
    puts("native Drop: NULL/OOM and diagnostic finalizers preserve C status/TLS plus independent deferred delivery: PASS");
    return 0;
}
