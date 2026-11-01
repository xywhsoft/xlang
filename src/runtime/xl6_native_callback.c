#include "src/common/xl6_xrt.h"
#include "xl6_native_callback.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct CallbackPayload {
    XL6_Closure* closure;
    XL6_CodeLease* adapter;
    XL6_ForeignView* owner;
} CallbackPayload;
struct XL6_NativeCallback {
    xatomic64 refs;
    uint64_t signature;
    const void* claim;
    bool cleared;
    CallbackPayload payload,retired;
};
static void mutation_begin(xrtownershipscope* scope)
{ if(!xrtOwnershipMutationBegin(scope))abort(); }
static void mutation_end(xrtownershipscope* scope)
{ if(!xrtOwnershipScopeEnd(scope))abort(); }
static bool hold_body(XL6_NativeCallback* record)
{
    if(!record || record->cleared || record->claim)return false;
    uint64 refs=xrtAtomic64Load(&record->refs,XMEMORY_RELAXED);
    do{if(!refs || refs==UINT64_MAX)return false;}
    while(!xrtAtomic64CompareExchange(&record->refs,&refs,refs+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    return true;
}
static bool hold_native(const void* raw)
{
    xrtownershipscope mutation={0};mutation_begin(&mutation);
    bool ok=hold_body((XL6_NativeCallback*)raw);mutation_end(&mutation);return ok;
}
static XL6_NativeCallback* retain(XL6_NativeCallback* record)
{
    if(!record)return NULL;
    if(hold_native(record))return record;
    xrtSetErrorKind(XERR_STATE);return NULL;
}
static bool release_payload(CallbackPayload payload)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBeginCleanup(&errors,false))abort();
    XL6_CodeScope adapter={0};xl6CodeScopeEnter(&adapter,payload.adapter);
    xl6ClosureRuntimeV1Get()->Release(payload.closure);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6ForeignViewNativeUnpinV1(payload.owner);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6CodeLeaseRelease(payload.adapter);xl6CodeScopeLeave(&adapter);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(!result.ok)xrtSetErrorTake(result.error);
    return result.ok;
}
static bool release(XL6_NativeCallback* record)
{
    if(!record)return true;
    xrtownershipscope mutation={0};mutation_begin(&mutation);
    uint64 refs=xrtAtomic64FetchSub(&record->refs,1,XMEMORY_ACQ_REL);if(!refs)abort();
    if(refs!=1){mutation_end(&mutation);return true;}
    if(record->retired.closure || record->retired.adapter || record->retired.owner)abort();
    CallbackPayload payload=record->payload;record->payload=(CallbackPayload){0};
    mutation_end(&mutation);xrtFree(record);return release_payload(payload);
}
static bool drop_native(const void* raw)
{
    if(!raw)return true;
    XL6_ForeignCallbackScopeV1 callback={0};if(!xl6ForeignCallbackEnterV1(&callback))abort();
    bool ok=release((XL6_NativeCallback*)raw);
    if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    if(!xl6ForeignCallbackLeaveV1(&callback))abort();
    return ok;
}
static XL6_NativeCallback* create(const XL6_CodeOwner* owner,XL6_Closure* closure,XL6_ForeignView* context,uint64_t signature)
{
    if(!closure || !signature || !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(closure))){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    XL6_NativeCallback* record=xrtCalloc(1,sizeof(*record));
    if(record){
        XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,&record->payload.adapter);
        if(admission!=XL6_CODE_LEASE_ACQUIRED){
            if(admission!=XL6_CODE_LEASE_RESOURCE_FAILURE && !xrtGetError())xrtSetErrorKind(XERR_STATE);
            xl6ErrorScopeCaptureFailure(&errors);
        }else if(context && !(record->payload.owner=xl6ForeignViewNativePinV1(context)))xl6ErrorScopeCaptureFailure(&errors);
        else record->payload.closure=xl6ClosureRuntimeV1Get()->Retain(closure);
        if(!record->payload.closure || xrtGetError() || errors.failed){
            xl6ErrorScopeCaptureFailure(&errors);(void)release_payload(record->payload);xrtFree(record);record=NULL;
        }else{xrtAtomic64Init(&record->refs,1);record->signature=signature;}
    }else xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);if(!result.ok)xrtSetErrorTake(result.error);
    return record;
}
static bool count(const void* raw,size_t* output)
{
    const XL6_NativeCallback* record=raw;
    if(!record || record->cleared || !output)return false;
    uint64 refs=xrtAtomic64Load(&record->refs,XMEMORY_RELAXED);
    if(!refs || refs>SIZE_MAX)return false;
    *output=(size_t)refs;return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{
    const XL6_NativeCallback* record=raw;
    if(!record || record->cleared || !visit)return false;
    const CallbackPayload* p=&record->payload;
    return (!p->closure || visit(xl6ClosureRuntimeV1Get()->Ownership(p->closure),user)) &&
        (!p->adapter || visit(xl6CodeLeaseOwnership(p->adapter),user)) &&
        (!p->owner || visit(xl6ForeignViewOwnershipV1(p->owner),user));
}
static const xrtownershipops ops={count,trace};
static xrtownershipref ownership(const XL6_NativeCallback* record)
{return (xrtownershipref){record,record?&ops:NULL};}
static bool scope_empty(const XL6_NativeCallbackScopeV1* scope)
{
    return scope && !scope->self && !scope->record && !scope->callback.self && !scope->callback.previous && !scope->callback.thread &&
        !scope->adapter.lease && !scope->adapter.previous && !scope->adapter.next && !scope->adapter.threadId &&
        !scope->closure.closure && !scope->closure.code.lease && !scope->closure.code.previous && !scope->closure.code.next && !scope->closure.code.threadId &&
        !scope->owner.self && !scope->owner.view && !scope->owner.root && !scope->owner.raw && !scope->owner.nativeStorage &&
        !scope->owner.execution.self && !scope->owner.execution.older && !scope->owner.execution.newer && !scope->owner.execution.owner;
}
static bool enter(XL6_NativeCallback* record,uint64_t signature,XL6_NativeCallbackScopeV1* scope,void* storage,size_t bytes)
{
    if(!record || signature!=record->signature || !scope_empty(scope)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!retain(record))return false;
    if(!xl6ForeignCallbackEnterV1(&scope->callback)){(void)release(record);return false;}
    xl6CodeScopeEnter(&scope->adapter,record->payload.adapter);
    xl6ClosureRuntimeV1Get()->Enter(&scope->closure,record->payload.closure);
    bool ok=record->payload.owner ? xl6ForeignViewCallBeginV1(record->payload.owner,&scope->owner,storage,bytes) : !storage && !bytes;
    if(!ok && !record->payload.owner)xrtSetErrorKind(XERR_ARGUMENT);
    if(!ok){
        xerror* error=xrtTakeError();
        xl6ClosureRuntimeV1Get()->Leave(&scope->closure);xl6CodeScopeLeave(&scope->adapter);
        if(!xl6ForeignCallbackLeaveV1(&scope->callback))abort();
        (void)release(record);xrtClearError();xrtSetErrorTake(error);return false;
    }
    scope->self=scope;scope->record=record;return true;
}
static xl6_error_result leave(XL6_NativeCallbackScopeV1* scope,xl6_error_result incoming)
{
    if(!scope || scope->self!=scope || !scope->record)abort();
    XL6_NativeCallback* record=scope->record;scope->self=NULL;scope->record=NULL;
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))abort();
    xrtSetErrorTake(incoming.error);
    if(!incoming.ok)xl6ErrorScopeCaptureFailure(&errors);
    if(scope->owner.self && !xl6ForeignViewCallEndV1(&scope->owner))xl6ErrorScopeCaptureFailure(&errors);
    xl6ClosureRuntimeV1Get()->Leave(&scope->closure);xl6CodeScopeLeave(&scope->adapter);
    if(!release(record) || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(!xl6ForeignCallbackLeaveV1(&scope->callback))abort();
    return xl6ErrorScopeEnd(&errors);
}
static bool collector_hold(const void* raw)
{
    XL6_NativeCallback* record=(XL6_NativeCallback*)raw;
    return hold_body(record);
}
static void collector_drop(const void* raw)
{if(!release((XL6_NativeCallback*)raw))xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static bool claim(const void* raw,const void* token)
{
    XL6_NativeCallback* record=(XL6_NativeCallback*)raw;
    if(!token || record->cleared || (record->claim && record->claim!=token))return false;
    record->claim=token;return true;
}
static void restore(const void* raw,const void* token)
{
    XL6_NativeCallback* record=(XL6_NativeCallback*)raw;
    if(record->claim!=token || record->cleared)abort();
    record->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    XL6_NativeCallback* record=(XL6_NativeCallback*)raw;
    if(record->claim!=token || record->cleared)abort();
    record->retired=record->payload;record->payload=(CallbackPayload){0};record->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    XL6_NativeCallback* record=(XL6_NativeCallback*)raw;
    if(record->claim!=token || !record->cleared)abort();
    if(!record->retired.closure && !record->retired.adapter && !record->retired.owner)return true;
    CallbackPayload payload=record->retired;record->retired=(CallbackPayload){0};return release_payload(payload);
}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)preparation;(void)completion;
    static const xrtownershipadapterv1 value={sizeof(value),collector_hold,collector_drop,claim,restore,NULL,clear,finish};
    if(ref.Ops!=&ops || !ref.Data)return false;
    const XL6_NativeCallback* record=ref.Data;
    if(record->cleared || !record->signature || !record->payload.closure ||
        !xrtAtomic64Load(&record->refs,XMEMORY_ACQUIRE))return false;
    *adapter=&value;return true;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainNativeCallbackV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,resolve,NULL};return &domain;}
const XL6_NativeCallbackRuntimeV1* xl6NativeCallbackRuntimeV1Get(void)
{
    static const XL6_NativeCallbackRuntimeV1 api={create,retain,release,ownership,enter,leave,hold_native,drop_native};return &api;
}
