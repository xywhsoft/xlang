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
    XL6_OwnershipProbeV1 probe;
    xmutex* lock;
    size_t leases, pins;
    bool entry, closing, running, cleared, finished, prepared, blocked;
    xatomic32 failed;
    unsigned long long driverThread;
    unsigned phase;
    const void* claim;
    xerror* firstError;
    XL6_OwnershipPlan* plan;
};
/* Admission, not a made-up owning root. A reader and semantic retirement are
 * mutually exclusive; callbacks see BUSY rather than waiting on themselves.
 * The owning reference counts remain unchanged. No allocating TLS/registry. */
enum { STATIC_BOUNDARY_READ=1,STATIC_BOUNDARY_RETIRE=2,STATIC_BOUNDARY_FINALIZER_READ=3 };
static xatomic32 staticBoundaryGate={0};
static xatomic64 staticBoundaryThread={0};
static XL6_OwnershipReadScopeV1* staticReadScope;
static const XL6_StaticModule* staticReadModule;
static bool staticBoundaryEnter(uint32 mode)
{
    uint32 expected=0;
    if(!xrtAtomic32CompareExchange(&staticBoundaryGate,&expected,mode,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return false;
    xrtAtomic64Store(&staticBoundaryThread,xrtThreadCurrentId(),XMEMORY_RELEASE);return true;
}
static void staticBoundaryLeave(void)
{
    xrtAtomic64Store(&staticBoundaryThread,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&staticBoundaryGate,0,XMEMORY_RELEASE);
}
static bool staticReadActive(void)
{
    /* Only this exact thread may touch the non-atomic scope pointer. The
     * global admission gate excludes replacement until its same-thread End. */
    return xrtAtomic64Load(&staticBoundaryThread,XMEMORY_ACQUIRE)==xrtThreadCurrentId() &&
        (xrtAtomic32Load(&staticBoundaryGate,XMEMORY_ACQUIRE)==STATIC_BOUNDARY_READ ||
         xrtAtomic32Load(&staticBoundaryGate,XMEMORY_ACQUIRE)==STATIC_BOUNDARY_FINALIZER_READ) &&
        staticReadScope && staticReadScope->self==staticReadScope && staticReadScope->thread==xrtThreadCurrentId();
}
static bool staticReadQuiescent(const XL6_StaticModule* module)
{
    return staticReadActive() && module->contract.read &&
        (!module->running || (module->driverThread==xrtThreadCurrentId() && module->phase==STATIC_FINALIZE)) &&
        !module->cleared && !module->finished &&
        !xl6CodeOwnerScopeForeign(&module->owner.base);
}
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
       (xl6CodeOwnerScopeActive(&module->owner.base) && !staticReadQuiescent(module)) || module->leases>SIZE_MAX-module->pins ||
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
int xl6StaticModuleReadBeginV1(XL6_StaticModule* module,XL6_OwnershipReadScopeV1* scope)
{
    if(!module || !module->contract.read || !scope || scope->self || scope->thread)return XL6_OWNERSHIP_READ_ERROR_V1;
    if(!staticBoundaryEnter(STATIC_BOUNDARY_READ)){
        if(xrtAtomic64Load(&staticBoundaryThread,XMEMORY_ACQUIRE)!=xrtThreadCurrentId() ||
            xrtAtomic32Load(&staticBoundaryGate,XMEMORY_ACQUIRE)!=STATIC_BOUNDARY_RETIRE ||
            !module->running || module->driverThread!=xrtThreadCurrentId() || module->phase!=STATIC_FINALIZE)
            return XL6_OWNERSHIP_READ_BUSY_V1;
        /* The semantic callback is outside Freeze. Preserve its exact driver
         * admission while synchronously inspecting intact fields; no nested
         * retirement, counter discount, unlock or foreign-thread permission. */
        xrtAtomic32Store(&staticBoundaryGate,STATIC_BOUNDARY_FINALIZER_READ,XMEMORY_RELEASE);
    }
    scope->self=scope;scope->thread=xrtThreadCurrentId();staticReadScope=scope;staticReadModule=module;
    return XL6_OWNERSHIP_READ_READY_V1;
}
bool xl6StaticModuleReadAdmitsV1(const XL6_StaticModule* module,const XL6_OwnershipReadScopeV1* scope,xrtownershipref ref)
{
    if(!staticReadActive() || scope!=staticReadScope || module!=staticReadModule || !ref.Data || !staticReadQuiescent(module))return false;
    if(ref.Ops==&staticOps){
        size_t count;return ref.Data==module && staticCount(module,&count);
    }
    /* A graph follows a module lease before reaching its opaque field family.
     * Inspect only certified descriptors from that exact implementation. The
     * independent callback is installed by the executable deployment contract. */
    return module->contract.admitsRead && module->contract.admitsRead(ref,module->contract.context);
}
void xl6StaticModuleReadEndV1(XL6_StaticModule* module,XL6_OwnershipReadScopeV1* scope)
{
    if(!staticReadActive() || scope!=staticReadScope || module!=staticReadModule)abort();
    bool nested=xrtAtomic32Load(&staticBoundaryGate,XMEMORY_ACQUIRE)==STATIC_BOUNDARY_FINALIZER_READ;
    staticReadScope=NULL;staticReadModule=NULL;*scope=(XL6_OwnershipReadScopeV1){0};
    if(nested)xrtAtomic32Store(&staticBoundaryGate,STATIC_BOUNDARY_RETIRE,XMEMORY_RELEASE);
    else staticBoundaryLeave();
}
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
static xrtownershipfinishresult staticFinish(const void* data,const void* token)
{
    XL6_StaticModule* module=(XL6_StaticModule*)data;
    if(module->claim!=token || !module->cleared)abort();
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return XRT_OWNERSHIP_FINISH_FAILED;
    XL6_ModuleRetireResult status=module->contract.step ? module->contract.step() :
        module->contract.graph->finish() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED;
    if(status!=XL6_MODULE_RETIRE_READY && status!=XL6_MODULE_RETIRE_BUSY && status!=XL6_MODULE_RETIRE_FAILED){
        xrtSetErrorKind(XERR_INTERNAL);status=XL6_MODULE_RETIRE_FAILED;
    }
    if(status==XL6_MODULE_RETIRE_FAILED)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return XRT_OWNERSHIP_FINISH_FAILED;}
    xrtErrorFree(outcome.error);
    if(status==XL6_MODULE_RETIRE_BUSY)return XRT_OWNERSHIP_FINISH_BUSY;
    /* Finish is outside Freeze. Publish the owning-slot state in Mutation. */
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    module->finished=true;if(!xrtOwnershipScopeEnd(&mutation))abort();return XRT_OWNERSHIP_FINISH_READY;
}
static bool staticFinishLegacy(const void* data,const void* token)
{ return staticFinish(data,token)==XRT_OWNERSHIP_FINISH_READY; }
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
    return staticCount(data,&count) && (!module->contract.discover || module->contract.discover(&module->owner.base,visit,context));
}
static XL6_OwnershipAdmissionV1 staticProbe(xrtownershipref ref,void* data,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    XL6_StaticModule* module=data;*adapter=NULL;*preparation=NULL;*completion=NULL;
    if(ref.Ops==&staticOps){
        if(ref.Data!=module)return XL6_OWNERSHIP_ADMISSION_REFUSED;
        /* A previously admitted callback can enter between the driver's
         * initial quiescence probe and the plan's fresh Freeze. Preserve the
         * physical Count refusal, but classify that exact active stack here
         * before seed Hold or discovery can mistake it for a broken graph. */
        if((module->running && module->driverThread!=xrtThreadCurrentId()) ||
            (xl6CodeOwnerScopeActive(&module->owner.base) && !staticReadQuiescent(module)))
            return XL6_OWNERSHIP_ADMISSION_BUSY;
        return staticResolve(ref,data,adapter,preparation,completion)
            ? XL6_OWNERSHIP_ADMISSION_ADMITTED : XL6_OWNERSHIP_ADMISSION_REFUSED;
    }
    return module->probe(ref,module->contract.context,adapter,preparation,completion);
}
XL6_StaticModule* xl6StaticModuleCreateV1(const XL6_StaticModuleContractV1* contract)
{
    if(!contract || contract->size!=sizeof(*contract) || !contract->graph ||
       contract->graph->size!=sizeof(*contract->graph) || contract->graph->abiTag!=0x584C364D525431ULL ||
       !contract->graph->trace || !contract->graph->finish || !contract->resolve ||
       (contract->read && (contract->read->size!=sizeof(*contract->read) || contract->read->abiTag!=0x584C364D524431ULL ||
        contract->read->trace!=contract->graph->trace)) || (!contract->read && contract->admitsRead)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    XL6_StaticModule* module=xrtCalloc(1,sizeof(*module));if(!module)return NULL;
    xrtAtomic32Init(&module->failed,0);
    module->lock=xrtMutexCreate();if(!module->lock){xrtFree(module);return NULL;}
    module->contract=*contract;module->entry=true;
    module->owner=(XL6_CodeOwnerGraph){{sizeof(module->owner),module,staticAcquire,staticRelease},0x584C36434F574E31ULL,&staticOps};
    return module;
}
XL6_StaticModule* xl6StaticModuleCreateV2(const XL6_StaticModuleContractV2* contract)
{
    if(!contract || contract->size!=sizeof(*contract) || !contract->base || !contract->probe){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_StaticModule* module=xl6StaticModuleCreateV1(contract->base);
    if(module)module->probe=contract->probe;
    return module;
}
const XL6_CodeOwner* xl6StaticModuleOwnerV1(const XL6_StaticModule* module)
{ return module ? &module->owner.base : NULL; }
xrtownershipref xl6StaticModuleOwnershipV1(const XL6_StaticModule* module)
{ return (xrtownershipref){module,module ? &staticOps : NULL}; }
static XL6_StaticModuleResultV1 staticRetire(XL6_StaticModule* module,bool* blockedFailure)
{
    /* A sticky semantic failure is independent of THIS driver's physical
     * progress. In particular ROOTED may only be a genuine native/code tail
     * which DrainFor must keep waiting for, even after an earlier failure. */
    if(blockedFailure)*blockedFailure=false;
    if(!module){xrtSetErrorKind(XERR_ARGUMENT);return (XL6_StaticModuleResultV1){false,false};}
    xrtownershipscope freeze={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
    if(module->running){if(!xrtOwnershipScopeEnd(&freeze))abort();return (XL6_StaticModuleResultV1){false,!staticFailed(module)};}
    module->blocked=false;module->closing=true;module->entry=false;
    if(xl6CodeOwnerScopeActive(&module->owner.base)){
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
    }
    module->running=true;module->driverThread=xrtThreadCurrentId();if(!xrtOwnershipScopeEnd(&freeze))abort();
    xl6_error_scope scope={0};bool begun=xl6ErrorScopeBegin(&scope);
    if(begun){
        if(module->phase==STATIC_START){
            XL6_OwnershipPlanResult status=module->probe
                ? xl6OwnershipPlanTryCreateV6(xl6StaticModuleOwnershipV1(module),staticProbe,staticDiscover,true,module,&module->plan)
                : xl6OwnershipPlanTryCreateV5(xl6StaticModuleOwnershipV1(module),staticResolve,staticDiscover,module,&module->plan);
            if(status==XL6_OWNERSHIP_PLAN_ERROR){module->blocked=true;xl6ErrorScopeCaptureFailure(&scope);goto end;}
            if(status==XL6_OWNERSHIP_PLAN_BUSY)goto end;
            if(status==XL6_OWNERSHIP_PLAN_ROOTED)module->blocked=true;
            module->phase=status==XL6_OWNERSHIP_PLAN_ROOTED ? STATIC_ABORT : status==XL6_OWNERSHIP_PLAN_PREPARATIONS ? STATIC_PREPARE : STATIC_FINALIZE;
        }
        if(module->phase==STATIC_PREPARE || module->phase==STATIC_FINALIZE){
            bool preparing=module->phase==STATIC_PREPARE;
            XL6_OwnershipStepResult status=preparing ? xl6OwnershipPlanPrepare(module->plan) : xl6OwnershipPlanFinalizeV2(module->plan);
            if(status==XL6_OWNERSHIP_STEP_FAILED || status==XL6_OWNERSHIP_STEP_ERROR){
                if(preparing || status==XL6_OWNERSHIP_STEP_ERROR)module->blocked=true;
                xl6ErrorScopeCaptureFailure(&scope);module->phase=STATIC_ABORT;
            }else if(status==XL6_OWNERSHIP_STEP_BUSY)goto end;
            else module->phase=preparing ? STATIC_FINALIZE : STATIC_COMMIT;
        }
        if(module->phase==STATIC_COMMIT){
            XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCommit(module->plan);
            if(status==XL6_OWNERSHIP_PLAN_ERROR){module->blocked=true;xl6ErrorScopeCaptureFailure(&scope);module->phase=STATIC_ABORT;}
            else if(status==XL6_OWNERSHIP_PLAN_ROOTED){module->blocked=true;module->phase=STATIC_ABORT;}
            else if(status==XL6_OWNERSHIP_PLAN_FINALIZERS)module->phase=STATIC_FINALIZE;
            else if(status==XL6_OWNERSHIP_PLAN_PREPARATIONS)module->phase=STATIC_PREPARE;
            else if(status==XL6_OWNERSHIP_PLAN_READY)module->phase=STATIC_FINISH;
        }
        if(module->phase==STATIC_FINISH){
            XL6_OwnershipStepResult status=xl6OwnershipPlanFinishV2(module->plan);
            if(status==XL6_OWNERSHIP_STEP_FAILED || status==XL6_OWNERSHIP_STEP_ERROR){module->blocked=true;xl6ErrorScopeCaptureFailure(&scope);}
            else if(status==XL6_OWNERSHIP_STEP_READY)module->phase=STATIC_DESTROY;
        }
        if(module->phase==STATIC_DESTROY || module->phase==STATIC_ABORT){
            bool committed=module->phase==STATIC_DESTROY;
            XL6_OwnershipPlanResult status=xl6OwnershipPlanTryDestroy(module->plan);
            if(status==XL6_OWNERSHIP_PLAN_READY){module->plan=NULL;module->phase=committed ? STATIC_DONE : STATIC_START;}
            else if(status==XL6_OWNERSHIP_PLAN_ERROR){module->blocked=true;xl6ErrorScopeCaptureFailure(&scope);}
        }
    end:;
    }
    if(!begun)module->blocked=true;
    xl6_error_result outcome=begun ? xl6ErrorScopeEnd(&scope) : (xl6_error_result){false,xrtTakeError()};
    if(blockedFailure)*blockedFailure=module->blocked && !outcome.ok;
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
static bool staticDestroy(XL6_StaticModule** pointer)
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
XL6_StaticModuleResultV1 xl6StaticModuleRetireV1(XL6_StaticModule* module)
{
    if(!module){xrtSetErrorKind(XERR_ARGUMENT);return (XL6_StaticModuleResultV1){false,false};}
    if(!staticBoundaryEnter(STATIC_BOUNDARY_RETIRE))return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
    XL6_StaticModuleResultV1 result=staticRetire(module,NULL);staticBoundaryLeave();return result;
}
XL6_StaticModuleResultV1 xl6StaticModuleDrainV1(XL6_StaticModule* module)
{
    if(!module){xrtSetErrorKind(XERR_ARGUMENT);return (XL6_StaticModuleResultV1){false,false};}
    for(;;){
        if(!staticBoundaryEnter(STATIC_BOUNDARY_RETIRE)){
            if(xrtAtomic64Load(&staticBoundaryThread,XMEMORY_ACQUIRE)==xrtThreadCurrentId())
                return (XL6_StaticModuleResultV1){false,!staticFailed(module)};
            xrtThreadYield();continue;
        }
        XL6_StaticModuleResultV1 result=staticRetire(module,NULL);bool blocked=module->blocked;
        staticBoundaryLeave();if(result.ready || blocked)return result;
        xrtThreadYield();
    }
}
XL6_StaticModuleResultV1 xl6StaticModuleDrainForV1(XL6_StaticModule* module,long long milliseconds)
{
    if(!module || milliseconds< -1){xrtSetErrorKind(XERR_ARGUMENT);return (XL6_StaticModuleResultV1){false,false};}
    uint64 timeout=milliseconds<0 || (uint64)milliseconds>UINT64_MAX/1000u ? UINT64_MAX : (uint64)milliseconds*1000u;
    xdeadline deadline=xrtDeadlineAfter(timeout);
    for(;;){
        XL6_StaticModuleResultV1 result={false,!staticFailed(module)};
        if(!staticBoundaryEnter(STATIC_BOUNDARY_RETIRE)){
            if(xrtAtomic64Load(&staticBoundaryThread,XMEMORY_ACQUIRE)==xrtThreadCurrentId())return result;
        }else{
            bool failed=false;result=staticRetire(module,&failed);
            staticBoundaryLeave();if(result.ready || failed)return result;
        }
        if(milliseconds>=0 && xrtDeadlineExpired(deadline))return result;
        xrtThreadYield();
    }
}
bool xl6StaticModuleDestroyV1(XL6_StaticModule** pointer)
{
    if(!staticBoundaryEnter(STATIC_BOUNDARY_RETIRE))return false;
    bool ok=staticDestroy(pointer);staticBoundaryLeave();return ok;
}
