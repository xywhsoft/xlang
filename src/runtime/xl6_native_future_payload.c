#define XRT_MODULE_MEMORY
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_TYPE
#include "src/common/xl6_xrt.h"
#include "xl6_native_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <stdlib.h>

typedef struct NativePayload {
    const XL6_NativeCaptureProfileV1* profile;
    const xrttype* type;
    size_t bytes;
    max_align_t alignment;
    unsigned char data[];
} NativePayload;
_Static_assert(offsetof(NativePayload,data)%_Alignof(max_align_t)==0,
    "native Future payload must preserve maximum C alignment");
static NativePayload* header(const void* data)
{ return (NativePayload*)((unsigned char*)data - offsetof(NativePayload,data)); }
static bool valid(const NativePayload* p)
{
    return p && xl6NativeCaptureProfileValidV1(p->profile) && p->type==p->profile->Type() &&
        p->bytes==p->type->Size && p->bytes<=SIZE_MAX - offsetof(NativePayload,data) &&
        (uintptr_t)p->data%p->type->Align==0 &&
        !xrtMemRangesOverlap(p->data,p->bytes,p->profile,sizeof(*p->profile)) &&
        !xrtMemRangesOverlap(p->data,p->bytes,p->type,sizeof(*p->type)) &&
        !xrtMemRangesOverlap(p->data,p->bytes,p->type->Ops,sizeof(*p->type->Ops));
}
static void* allocate(const XL6_NativeCaptureProfileV1* profile)
{
    if(!xl6NativeCaptureProfileValidV1(profile)) { xrtSetErrorKind(XERR_ARGUMENT);return NULL; }
    const xrttype* type=profile->Type();const size_t prefix=offsetof(NativePayload,data);
    if(type->Size>SIZE_MAX-prefix) { xrtSetErrorKind(XERR_RANGE);return NULL; }
    NativePayload* p=xrtMalloc(prefix+type->Size);if(!p)return NULL;
    *p=(NativePayload){.profile=profile,.type=type,.bytes=type->Size};
    memset(p->data,0,type->Size);return p->data;
}
static void drop(void* data,void* unused)
{
    (void)unused;if(!data)return;
    NativePayload* p=header(data);if(!valid(p))abort();
    p->profile->DetachCleanup(data,NULL);xrtFree(p);
}
static bool trace(const void* data,const void* unused,xrtownershipvisitor visit,void* context)
{ (void)context;return !unused && visit && data && valid(header(data)); }
static const xfuturepayloadownershipv1 policy={sizeof(policy),drop,trace};
static const xfuturepayloadownershipv1* get_policy(void) { return &policy; }
static bool resolve(xpromise* promise,void* data)
{
    if(!data || !valid(header(data))) { xrtSetErrorKind(XERR_ARGUMENT);return false; }
    return xrtPromiseResolveOwnedPolicyV1(promise,data,&policy);
}
static bool copy(const void* data,const XL6_NativeCaptureProfileV1* expected,void* output)
{
    xl6_error_scope scope={0};bool copied=false;
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;
    }
    const NativePayload* p=data ? header(data) : NULL;
    if(!p || !valid(p) || p->profile!=expected || !output ||
        (uintptr_t)output%p->type->Align ||
        xrtMemRangesOverlap(p,offsetof(NativePayload,data)+p->bytes,output,p->bytes) ||
        xrtMemRangesOverlap(p->profile,sizeof(*p->profile),output,p->bytes) ||
        xrtMemRangesOverlap(p->type,sizeof(*p->type),output,p->bytes) ||
        xrtMemRangesOverlap(p->type->Ops,sizeof(*p->type->Ops),output,p->bytes))
        xrtSetErrorKind(XERR_ARGUMENT);
    else {
        copied=xrtTypeCopyValue(p->type,output,data);
        if(!copied || xrtGetError() || scope.failed) {
            xl6ErrorScopeCaptureFailure(&scope);p->profile->DetachCleanup(output,NULL);copied=false;
        }
    }
    if(!copied || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static const xrtownershipadapterv1* ownership_adapter(xrtownershipref ref)
{
    const xfuturepayloadownershipv1* const allowed[]={&policy};
    return xrtFutureOwnershipAdapterV1(ref,allowed,1);
}
const XL6_NativeFuturePayloadRuntimeV1* xl6NativeFuturePayloadRuntimeV1Get(void)
{
    static const XL6_NativeFuturePayloadRuntimeV1 api={allocate,drop,resolve,copy,get_policy,ownership_adapter};
    return &api;
}
