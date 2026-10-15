#define XRT_MODULE_MEMORY
#define XRT_MODULE_MUTEX
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE_GRAPH
#include "lib/xrt/xrt.h"
#include "xl6_static_module.h"
#include "xl6_code_lease.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

enum { STATIC_START, STATIC_PREPARE, STATIC_FINALIZE, STATIC_COMMIT,
       STATIC_FINISH, STATIC_DESTROY, STATIC_ABORT, STATIC_DONE };
struct XL6_StaticModule {
    XL6_CodeOwnerGraph owner;
    XL6_StaticModuleContractV1 contract;
    xmutex* lock;
    size_t leases, pins;
    bool entry, closing, running, cleared, finished, prepared;
    xatomic32 failed;
    unsigned long long driverThread;
    unsigned phase;
    const void* claim;
    xerror* firstError;
    XL6_OwnershipPlan* plan;
};
static bool staticFailed(const XL6_StaticModule* module)
{ return xrtAtomic32Load(&module->failed,XMEMORY_ACQUIRE)!=0; }
static int staticAcquire(void* data)
{
    XL6_StaticModule* module=data;int ok=0;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return 0;
    if(xrtMutexLock(module->lock)){
        if(!module->closing && module->leases<SIZE_MAX-module->pins-1u){++module->leases;ok=1;}
        if(!xrtMutexUnlock(module->lock))abort();
    }
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return ok;
}
static void staticRelease(void* data)
{
    XL6_StaticModule* module=data;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!xrtMutexLock(module->lock))abort();
    if(!module->leases)abort();
    --module->leases;
    if(!xrtMutexUnlock(module->lock) || !xrtOwnershipScopeEnd(&mutation))abort();
}
/* All physical transitions use Mutation; a snapshot reads only under Freeze.
 * The running driver is an exclusive borrowed control record, NOT a made-up
 * owning slot to subtract. Its actual plan pins are counted independently. */
static bool staticCount(const void* data,size_t* count)
{
    const XL6_StaticModule* module=data;
    if(!module || !count || (module->running && module->driverThread!=xrtThreadCurrentId()) ||
       xl6CodeOwnerScopeActive(&module->owner.base) || module->leases>SIZE_MAX-module->pins ||
       (module->entry && module->leases+module->pins==SIZE_MAX))return false;
    *count=module->leases+module->pins+(module->entry ? 1u : 0u);return true;
}
static bool staticTrace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_StaticModule* module=data;size_t count;
    if(!visit || !staticCount(data,&count))return false;
    if(!visit(xrtErrorOwnership(module->firstError),context))return false;
    return module->finished || module->contract.graph->trace(visit,context);
}
static const xrtownershipops staticOps={staticCount,staticTrace};
static bool staticHold(const void* data)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;size_t count;
    if(!staticCount(data,&count) || count==SIZE_MAX)return false;
    ++module->pins;return true;
}
static void staticDrop(const void* data)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation) || !xrtMutexLock(module->lock))abort();
    if(!module->pins)abort();
    --module->pins;
    if(!xrtMutexUnlock(module->lock) || !xrtOwnershipScopeEnd(&mutation))abort();
}
static bool staticClaim(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(!module->closing || module->cleared || (module->claim && module->claim!=token))return false;
    module->claim=token;return true;
}
static void staticRestore(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(module->claim!=token || module->cleared)abort();
    module->claim=NULL;
}
static void staticClear(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(module->claim!=token || module->cleared)abort();
    module->cleared=true;
}
static XL6_OwnershipStepResult staticFinish(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(module->claim!=token || !module->cleared)abort();
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return XL6_OWNERSHIP_STEP_FAILED;
    XL6_ModuleRetireResult status=module->contract.step ? module->contract.step() :
        module->contract.graph->finish() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED;
    if(status!=XL6_MODULE_RETIRE_READY && status!=XL6_MODULE_RETIRE_BUSY && status!=XL6_MODULE_RETIRE_FAILED){
        xrtSetErrorKind(XERR_INTERNAL);status=XL6_MODULE_RETIRE_FAILED;
    }
    if(status==XL6_MODULE_RETIRE_FAILED)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return XL6_OWNERSHIP_STEP_FAILED;}
    xrtErrorFree(outcome.error);
    if(status==XL6_MODULE_RETIRE_BUSY)return XL6_OWNERSHIP_STEP_BUSY;
    /* Finish is outside Freeze. Publish the owning-slot state in Mutation. */
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    module->finished=true;if(!xrtOwnershipScopeEnd(&mutation))abort();return XL6_OWNERSHIP_STEP_READY;
}
static bool staticFinishLegacy(const void* data,const void* token)
{ return staticFinish(data,token)==XL6_OWNERSHIP_STEP_READY; }
static const xrtownershipadapterv1 staticAdapter={sizeof(staticAdapter),staticHold,staticDrop,
    staticClaim,staticRestore,NULL,staticClear,staticFinishLegacy};
static const XL6_OwnershipFinishV1 staticCompletion={sizeof(staticCompletion),&staticAdapter,staticFinish};
static bool staticPrepared(const void* data)
{ return ((const XL6_StaticModule*)data)->prepared; }
static xrtownershipprepareresult staticPrepare(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(module->claim!=token || module->cleared)abort();
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return XRT_OWNERSHIP_PREPARE_FAILED;
    XL6_ModulePrepareResult status=module->contract.preflight ? module->contract.preflight() : XL6_MODULE_PREPARE_READY;
    if(status!=XL6_MODULE_PREPARE_READY && status!=XL6_MODULE_PREPARE_BUSY && status!=XL6_MODULE_PREPARE_FAILED){
        xrtSetErrorKind(XERR_INTERNAL);status=XL6_MODULE_PREPARE_FAILED;
    }
    if(status==XL6_MODULE_PREPARE_FAILED)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return XRT_OWNERSHIP_PREPARE_FAILED;}
    xrtErrorFree(outcome.error);
    if(status==XL6_MODULE_PREPARE_BUSY)return XRT_OWNERSHIP_PREPARE_BUSY;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    module->prepared=true;if(!xrtOwnershipScopeEnd(&mutation))abort();return XRT_OWNERSHIP_PREPARE_READY;
}
static const xrtownershippreparationv1 staticPreparation={sizeof(staticPreparation),&staticAdapter,staticPrepared,staticPrepare};
static bool staticResolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    XL6_StaticModule* module=data;
    if(ref.Ops==&staticOps){
        if(ref.Data!=module)return false;
        *adapter=&staticAdapter;*preparation=&staticPreparation;*completion=&staticCompletion;return true;
    }
    return module->contract.resolve(ref,module->contract.context,adapter,preparation,completion);
}
static bool staticDiscover(XL6_ModuleOwnershipVisit visit,void* context,void* data)
{
    XL6_StaticModule* module=data;size_t count;
    return staticCount(data,&count) && xl6ClassOwnershipDiscoverV1(&module->owner.base,visit,context);
}
XL6_StaticModule* xl6StaticModuleCreateV1(const XL6_StaticModuleContractV1* contract)
{
    if(!contract || contract->size!=sizeof(*contract) || !contract->graph ||
       contract->graph->size!=sizeof(*contract->graph) || contract->graph->abiTag!=0x584C364D525431ULL ||
       !contract->graph->trace || !contract->graph->finish || !contract->resolve){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_StaticModule* module=xrtCalloc(1,sizeof(*module));if(!module)return NULL;
    xrtAtomic32Init(&module->failed,0);
    module->lock=xrtMutexCreate();if(!module->lock){xrtFree(module);return NULL;}
    module->contract=*contract;module->entry=true;
    module->owner=(XL6_CodeOwnerGraph){{sizeof(module->owner),module,staticAcquire,staticRelease},0x584C36434F574E31ULL,&staticOps};
    return module;
}
const XL6_CodeOwner* xl6StaticModuleOwnerV1(const XL6_StaticModule* module)
{ return module ? &module->owner.base : NULL; }
xrtownershipref xl6StaticModuleOwnershipV1(const XL6_StaticModule* module)
{ return (xrtownershipref){module,module ? &staticOps : NULL}; }
XL6_StaticModuleResultV1 xl6StaticModuleRetireV1(XL6_StaticModule* module)
{
    if(!module){xrtSetErrorKind(XERR_ARGUMENT);return (XL6_StaticModuleResultV1){false,false};}
    xrtownershipscope freeze={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
    if(module->running){if(!xrtOwnershipScopeEnd(&freeze))abort();return (XL6_StaticModuleResultV1){false,!staticFailed(module)};}
    module->closing=true;module->entry=false;
    if(xl6CodeOwnerScopeActive(&module->owner.base)){
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
    }
    module->running=true;module->driverThread=xrtThreadCurrentId();if(!xrtOwnershipScopeEnd(&freeze))abort();
    xl6_error_scope scope={0};bool begun=xl6ErrorScopeBegin(&scope);
    if(begun){
        if(module->phase==STATIC_START){
            XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV4(xl6StaticModuleOwnershipV1(module),staticResolve,staticDiscover,module,&module->plan);
            if(status==XL6_OWNERSHIP_PLAN_ERROR){xl6ErrorScopeCaptureFailure(&scope);goto end;}
            if(status==XL6_OWNERSHIP_PLAN_BUSY)goto end;
            module->phase=status==XL6_OWNERSHIP_PLAN_ROOTED ? STATIC_ABORT : status==XL6_OWNERSHIP_PLAN_PREPARATIONS ? STATIC_PREPARE : STATIC_FINALIZE;
        }
        if(module->phase==STATIC_PREPARE || module->phase==STATIC_FINALIZE){
            bool preparing=module->phase==STATIC_PREPARE;
            XL6_OwnershipStepResult status=preparing ? xl6OwnershipPlanPrepare(module->plan) : xl6OwnershipPlanFinalizeV2(module->plan);
            if(status==XL6_OWNERSHIP_STEP_FAILED || status==XL6_OWNERSHIP_STEP_ERROR){
                xl6ErrorScopeCaptureFailure(&scope);module->phase=STATIC_ABORT;
            }else if(status==XL6_OWNERSHIP_STEP_BUSY)goto end;
            else module->phase=preparing ? STATIC_FINALIZE : STATIC_COMMIT;
        }
        if(module->phase==STATIC_COMMIT){
            XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCommit(module->plan);
            if(status==XL6_OWNERSHIP_PLAN_ERROR){xl6ErrorScopeCaptureFailure(&scope);module->phase=STATIC_ABORT;}
            else if(status==XL6_OWNERSHIP_PLAN_ROOTED)module->phase=STATIC_ABORT;
            else if(status==XL6_OWNERSHIP_PLAN_FINALIZERS)module->phase=STATIC_FINALIZE;
            else if(status==XL6_OWNERSHIP_PLAN_PREPARATIONS)module->phase=STATIC_PREPARE;
            else if(status==XL6_OWNERSHIP_PLAN_READY)module->phase=STATIC_FINISH;
        }
        if(module->phase==STATIC_FINISH){
            XL6_OwnershipStepResult status=xl6OwnershipPlanFinishV2(module->plan);
            if(status==XL6_OWNERSHIP_STEP_FAILED || status==XL6_OWNERSHIP_STEP_ERROR)xl6ErrorScopeCaptureFailure(&scope);
            else if(status==XL6_OWNERSHIP_STEP_READY)module->phase=STATIC_DESTROY;
        }
        if(module->phase==STATIC_DESTROY || module->phase==STATIC_ABORT){
            bool committed=module->phase==STATIC_DESTROY;
            XL6_OwnershipPlanResult status=xl6OwnershipPlanTryDestroy(module->plan);
            if(status==XL6_OWNERSHIP_PLAN_READY){module->plan=NULL;module->phase=committed ? STATIC_DONE : STATIC_START;}
            else if(status==XL6_OWNERSHIP_PLAN_ERROR)xl6ErrorScopeCaptureFailure(&scope);
        }
    end:;
    }
    xl6_error_result outcome=begun ? xl6ErrorScopeEnd(&scope) : (xl6_error_result){false,xrtTakeError()};
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!outcome.ok && !staticFailed(module)){
        module->firstError=xrtErrorRef(outcome.error);
        xrtAtomic32Store(&module->failed,1,XMEMORY_RELEASE);
    }
    module->running=false;module->driverThread=0;
    XL6_StaticModuleResultV1 result={module->phase==STATIC_DONE,!staticFailed(module)};
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xrtErrorFree(outcome.error);
    if(!result.ok && !xrtGetError())xrtSetErrorTake(xrtErrorRef(module->firstError));
    return result;
}
bool xl6StaticModuleDestroyV1(XL6_StaticModule** pointer)
{
    if(!pointer || !*pointer){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_StaticModule* module=*pointer;xrtownershipscope freeze={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return false;
    bool ready=!module->running && module->phase==STATIC_DONE && !module->plan && !module->leases && !module->pins;
    if(!xrtOwnershipScopeEnd(&freeze))abort();
    if(!ready){xrtSetErrorKind(XERR_STATE);return false;}
    if(!xrtMutexDestroy(module->lock))return false;
    xrtErrorFree(module->firstError);xrtFree(module);*pointer=NULL;return true;
}
