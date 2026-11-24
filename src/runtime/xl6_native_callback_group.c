#include "src/common/xl6_xrt.h"
#include "xl6_native_callback_internal.h"
#include "xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

/* Owned callback groups share one publisher userdata pointer. Members remain
 * genuine native-callback roots, preserving their existing hot Enter/Leave. */
static void group_mutation_begin(xrtownershipscope* scope)
{if(!xrtOwnershipMutationBegin(scope))abort();}
static void group_mutation_end(xrtownershipscope* scope)
{if(!xrtOwnershipScopeEnd(scope))abort();}
static void group_report_drop_failure(void)
{
    xl6_error_result failure={false,xrtTakeError()};
    if(xl6ErrorScopeTryPropagateFailureV1(&failure))return;
    xerror* native=xrtErrorRef(failure.error);
    xl6CallbackFailureRecordV1(failure);xrtSetErrorTake(native);
}
typedef struct CallbackGroupPayload {
    XL6_NativeCallback* const* members;
    size_t count;
    XL6_CodeLease* adapter;
} CallbackGroupPayload;
typedef struct CallbackGroup {
    xatomic64 refs;
    const void* claim;
    bool cleared;
    XL6_CodeLease* adapter;
    CallbackGroupPayload retired;
    XL6_NativeCallbackGroupV1 view;
} CallbackGroup;
static bool group_count_fits(size_t count)
{
    return count && count <= (SIZE_MAX-sizeof(CallbackGroup))/sizeof(XL6_NativeCallback*);
}
static CallbackGroup* group_body(const void* view)
{return view ? (CallbackGroup*)((unsigned char*)view - offsetof(CallbackGroup,view)):NULL;}
static bool group_hold_body(CallbackGroup* body)
{
    if(!body || body->cleared || body->claim)return false;
    uint64 refs=xrtAtomic64Load(&body->refs,XMEMORY_RELAXED);
    do{if(!refs || refs==UINT64_MAX)return false;}
    while(!xrtAtomic64CompareExchange(&body->refs,&refs,refs+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    return true;
}
static bool group_hold_native(const void* raw)
{
    xrtownershipscope mutation={0};group_mutation_begin(&mutation);
    bool ok=group_hold_body(group_body(raw));group_mutation_end(&mutation);
    if(!ok)xrtSetErrorKind(XERR_STATE);
    return ok;
}
static XL6_NativeCallbackGroupV1* group_retain(XL6_NativeCallbackGroupV1* view)
{return group_hold_native(view) ? view:NULL;}
static bool group_release_payload(CallbackGroupPayload payload)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))abort();
    XL6_CodeScope adapter={0};xl6CodeScopeEnter(&adapter,payload.adapter);
    const XL6_NativeCallbackRuntimeV1* members=xl6NativeCallbackRuntimeV1Get();
    for(size_t i=payload.count;i>0;--i)
        if(!members->Release(payload.members[i-1]) || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6CodeLeaseRelease(payload.adapter);xl6CodeScopeLeave(&adapter);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(!result.ok)xrtSetErrorTake(result.error);
    return result.ok;
}
static bool group_release(XL6_NativeCallbackGroupV1* view)
{
    CallbackGroup* body=group_body(view);if(!body)return true;
    xrtownershipscope mutation={0};group_mutation_begin(&mutation);
    uint64 refs=xrtAtomic64FetchSub(&body->refs,1,XMEMORY_ACQ_REL);if(!refs)abort();
    if(refs!=1){group_mutation_end(&mutation);return true;}
    if(body->retired.members || body->retired.count || body->retired.adapter)abort();
    CallbackGroupPayload payload={body->view.Members,body->view.Count,body->adapter};
    body->view=(XL6_NativeCallbackGroupV1){0};body->adapter=NULL;
    group_mutation_end(&mutation);
    bool ok=group_release_payload(payload);xrtFree(body);return ok;
}
static bool group_drop_native(const void* raw)
{
    if(!raw)return true;
    XL6_ForeignCallbackScopeV1 callback={0};if(!xl6ForeignCallbackEnterV1(&callback))abort();
    bool ok=group_release((XL6_NativeCallbackGroupV1*)raw);
    if(!ok)group_report_drop_failure();
    if(!xl6ForeignCallbackLeaveV1(&callback))abort();
    return ok;
}
static XL6_NativeCallbackGroupV1* group_create(const XL6_CodeOwner* owner,
    XL6_Closure* const* closures,XL6_ForeignView* const* contexts,const uint64_t* signatures,size_t count)
{
    if(!group_count_fits(count) || !closures || !signatures){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    for(size_t i=0;i<count;++i)if(closures[i] && !signatures[i]){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    CallbackGroup* body=xrtCalloc(1,sizeof(*body)+count*sizeof(XL6_NativeCallback*));
    if(body){
        XL6_NativeCallback** members=(XL6_NativeCallback**)(body+1);
        body->view=(XL6_NativeCallbackGroupV1){count,members};
        XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(owner,&body->adapter);
        if(admission!=XL6_CODE_LEASE_ACQUIRED){
            if(admission!=XL6_CODE_LEASE_RESOURCE_FAILURE && !xrtGetError())xrtSetErrorKind(XERR_STATE);
            xl6ErrorScopeCaptureFailure(&errors);
        }
        for(size_t i=0;i<count && !errors.failed;++i)if(closures[i]){
            members[i]=xl6NativeCallbackCreateSharedAdapterV1(body->adapter,closures[i],contexts ? contexts[i]:NULL,signatures[i]);
            if(!members[i] || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
        }
        if(errors.failed || xrtGetError()){
            xl6ErrorScopeCaptureFailure(&errors);
            if(!group_release_payload((CallbackGroupPayload){members,count,body->adapter}))xl6ErrorScopeCaptureFailure(&errors);
            xrtFree(body);body=NULL;
        }else xrtAtomic64Init(&body->refs,1);
    }else xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(!result.ok)xrtSetErrorTake(result.error);
    return body ? &body->view:NULL;
}
static bool group_count(const void* raw,size_t* output)
{
    const CallbackGroup* body=group_body(raw);
    if(!body || body->cleared || !output)return false;
    uint64 refs=xrtAtomic64Load(&body->refs,XMEMORY_RELAXED);
    if(!refs || refs>SIZE_MAX)return false;
    *output=(size_t)refs;return true;
}
static bool group_trace(const void* raw,xrtownershipvisitor visit,void* user)
{
    const CallbackGroup* body=group_body(raw);
    if(!body || body->cleared || !visit)return false;
    const XL6_NativeCallbackRuntimeV1* members=xl6NativeCallbackRuntimeV1Get();
    if(body->adapter && !visit(xl6CodeLeaseOwnership(body->adapter),user))return false;
    for(size_t i=0;i<body->view.Count;++i)
        if(body->view.Members[i] && !visit(members->Ownership(body->view.Members[i]),user))return false;
    return true;
}
static const xrtownershipops group_ops={group_count,group_trace};
static xrtownershipref group_ownership(const XL6_NativeCallbackGroupV1* view)
{return (xrtownershipref){view,view ? &group_ops:NULL};}
static bool group_collector_hold(const void* raw)
{return group_hold_body(group_body(raw));}
static void group_collector_drop(const void* raw)
{if(!group_release((XL6_NativeCallbackGroupV1*)raw))group_report_drop_failure();}
static bool group_claim(const void* raw,const void* token)
{
    CallbackGroup* body=group_body(raw);
    if(!token || body->cleared || (body->claim && body->claim!=token))return false;
    body->claim=token;return true;
}
static void group_restore(const void* raw,const void* token)
{
    CallbackGroup* body=group_body(raw);
    if(body->claim!=token || body->cleared)abort();
    body->claim=NULL;
}
static void group_clear(const void* raw,const void* token)
{
    CallbackGroup* body=group_body(raw);
    if(body->claim!=token || body->cleared)abort();
    body->retired=(CallbackGroupPayload){body->view.Members,body->view.Count,body->adapter};
    body->view=(XL6_NativeCallbackGroupV1){0};body->adapter=NULL;body->cleared=true;
}
static bool group_finish(const void* raw,const void* token)
{
    CallbackGroup* body=group_body(raw);
    if(body->claim!=token || !body->cleared)abort();
    CallbackGroupPayload payload=body->retired;body->retired=(CallbackGroupPayload){0};
    if(!payload.members && !payload.count && !payload.adapter)return true;
    return group_release_payload(payload);
}
static bool group_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)preparation;(void)completion;
    static const xrtownershipadapterv1 value={sizeof(value),group_collector_hold,group_collector_drop,
        group_claim,group_restore,NULL,group_clear,group_finish};
    if(ref.Ops!=&group_ops || !ref.Data)return false;
    const CallbackGroup* body=group_body(ref.Data);
    if(body->cleared || !group_count_fits(body->view.Count) || !body->view.Members ||
        !xrtAtomic64Load(&body->refs,XMEMORY_ACQUIRE))return false;
    *adapter=&value;return true;
}
const XL6_NativeCallbackGroupRuntimeV1* xl6NativeCallbackGroupRuntimeV1Get(void)
{
    static const XL6_NativeCallbackGroupRuntimeV1 api={group_create,group_retain,group_release,
        group_ownership,group_hold_native,group_drop_native};return &api;
}

const XL6_OwnershipDomainV1* xl6OwnershipDomainNativeCallbackGroupV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,group_resolve,NULL};return &domain;}
