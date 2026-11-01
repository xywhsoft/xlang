#include "src/common/xl6_xrt.h"
#include "xl6_native_graph_future_payload.h"
#include "xl6_foreign_callback_scope.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct GraphPayload {
    const XL6_NativeGraphCaptureProfileV1* profile;
    size_t bytes;
    max_align_t alignment;
    unsigned char data[];
} GraphPayload;
_Static_assert(offsetof(GraphPayload,data)%_Alignof(max_align_t)==0,"graph Future payload alignment");
static GraphPayload* header(const void* data)
{ return (GraphPayload*)((unsigned char*)data - offsetof(GraphPayload,data)); }
static bool valid(const GraphPayload* p)
{
    const XL6_NativeGraphCaptureProfileV1* profile=p && p->profile ? xl6NativeGraphCaptureProfileV1Get(&p->profile->prefix) : NULL;
    return profile && profile==p->profile && p->bytes==profile->bytes &&
        p->bytes<=SIZE_MAX - offsetof(GraphPayload,data) && (uintptr_t)p->data%profile->alignment==0 &&
        !xrtMemRangesOverlap(p->data,p->bytes,profile,sizeof(*profile));
}
static void* allocate(const XL6_NativeCaptureProfileV1* prefix)
{
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(prefix);
    if (!profile) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (profile->bytes>SIZE_MAX - offsetof(GraphPayload,data)) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    GraphPayload* p=xrtCalloc(1,offsetof(GraphPayload,data)+profile->bytes);
    if (!p) return NULL;
    p->profile=profile; p->bytes=profile->bytes; return p->data;
}
static void drop(void* data,void* unused)
{
    if (!data) return;
    GraphPayload* p=header(data);
    if (unused || !valid(p)) abort();
    xl6_error_scope errors={0};
    if(!xl6ErrorScopeBeginCleanup(&errors,false))abort();
    XL6_ForeignCallbackScopeV1 callback={0};
    if (!xl6ForeignCallbackEnterV1(&callback)) abort();
    if(!p->profile->Detach(data,NULL))xl6ErrorScopeCaptureFailure(&errors);
    xrtFree(p);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    if (!xl6ForeignCallbackLeaveV1(&callback)) abort();
}
static bool trace(const void* data,const void* unused,xrtownershipvisitor visit,void* context)
{
    const GraphPayload* p=data ? header(data) : NULL;
    return !unused && visit && p && valid(p) && p->profile->Trace(data,visit,context);
}
static const xfuturepayloadownershipv1 policy={sizeof(policy),drop,trace};
static const xfuturepayloadownershipv1* get_policy(void) { return &policy; }
static bool resolve(xpromise* promise,void* data)
{
    if (!data || !valid(header(data))) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return xrtPromiseResolveOwnedPolicyV1(promise,data,&policy);
}
static bool copy(const void* data,const XL6_NativeCaptureProfileV1* expected,void* output)
{
    xl6_error_scope errors={0};
    if (!xl6ErrorScopeBegin(&errors))return false;
    const XL6_NativeGraphCaptureProfileV1* expectedProfile=xl6NativeGraphCaptureProfileV1Get(expected);
    const GraphPayload* p=data && expectedProfile ? header(data) : NULL;
    bool copied=false;
    if (!p || !valid(p) || expected!=&p->profile->prefix || !output ||
        (uintptr_t)output%p->profile->alignment ||
        xrtMemRangesOverlap(p,offsetof(GraphPayload,data)+p->bytes,output,p->bytes) ||
        xrtMemRangesOverlap(p->profile,sizeof(*p->profile),output,p->bytes)) {
        xrtSetErrorKind(XERR_ARGUMENT); xl6ErrorScopeCaptureFailure(&errors);
    } else {
        copied=p->profile->CopyFresh(output,data);
        if (!copied || xrtGetError() || errors.failed) {
            xl6ErrorScopeCaptureFailure(&errors);
            /* CopyFresh guarantees empty on failure. A reported failure after
             * successful acquisition still consumes that independent output. */
            if (copied && (!p->profile->Detach(output,NULL) || xrtGetError())) xl6ErrorScopeCaptureFailure(&errors);
        }
    }
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if (!result.ok) xrtSetErrorTake(result.error);
    return result.ok && copied;
}
static const xrtownershipadapterv1* ownership_adapter(xrtownershipref ref)
{
    const xfuturepayloadownershipv1* const allowed[]={&policy};
    return xrtFutureOwnershipAdapterV1(ref,allowed,1);
}
const XL6_NativeFuturePayloadRuntimeV1* xl6NativeGraphFuturePayloadRuntimeV1Get(void)
{
    static const XL6_NativeFuturePayloadRuntimeV1 api={allocate,drop,resolve,copy,get_policy,ownership_adapter};
    return &api;
}
