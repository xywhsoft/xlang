#include "src/common/xl6_xrt.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static xatomic32 accessLock;
static XL6_NativeHandleAccessV1* activeAccess;
static void access_lock(void)
{while(xrtAtomic32Exchange(&accessLock,1,XMEMORY_ACQUIRE))xrtThreadYield();}
static void access_unlock(void)
{xrtAtomic32Store(&accessLock,0,XMEMORY_RELEASE);}

bool xl6NativeHandleAccessBeginV1(xvalue* value,const XL6_NativeHandlePolicyV1* policy,
    XL6_NativeHandleAccessModeV1 mode,XL6_NativeHandleAccessV1* scope)
{
    if(!scope || (uintptr_t)scope % _Alignof(XL6_NativeHandleAccessV1) ||
        scope->self || scope->next || scope->previous || scope->value || scope->raw || scope->operations || scope->mode ||
        scope->execution.self || scope->execution.older || scope->execution.newer || scope->execution.owner ||
        !policy || policy->size!=sizeof(*policy) || policy->abiTag!=XL6_NATIVE_HANDLE_POLICY_V1_TAG ||
        !policy->Ops || !policy->Trace || mode<XL6_NATIVE_HANDLE_ACCESS_CALL_V1 || mode>XL6_NATIVE_HANDLE_ACCESS_TAKE_V1) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    void* raw=NULL;void* context=NULL;const xvaluehandleops* operations=NULL;
    if(!value || !xrtValueGetHandle(value,&raw,&operations,&context) || !raw || context || operations!=policy->Ops) {
        xrtSetErrorKind(XERR_TYPE);return false;
    }
    if(!xl6ForeignContextReadyV1() || !xl6ForeignContextCallEnterV1(&scope->execution,scope))return false;
    xvalue* held=xrtValueRetain(value);
    if(!held){xl6ForeignContextCallLeaveV1(&scope->execution);return false;}
    bool conflict=false;
    access_lock();
    for(const XL6_NativeHandleAccessV1* entry=activeAccess;entry;entry=entry->next) {
        if(entry->value!=value || entry->operations!=operations)continue;
        if(mode==XL6_NATIVE_HANDLE_ACCESS_TAKE_V1 || entry->mode==XL6_NATIVE_HANDLE_ACCESS_TAKE_V1 ||
            mode==XL6_NATIVE_HANDLE_ACCESS_WRITE_SPAN_V1 || entry->mode==XL6_NATIVE_HANDLE_ACCESS_WRITE_SPAN_V1 ||
            ((mode==XL6_NATIVE_HANDLE_ACCESS_CALL_V1)!=(entry->mode==XL6_NATIVE_HANDLE_ACCESS_CALL_V1))) {conflict=true;break;}
    }
    if(!conflict) {
        scope->self=scope;scope->value=held;scope->raw=raw;scope->operations=operations;scope->mode=mode;
        scope->next=activeAccess;scope->previous=&activeAccess;
        if(activeAccess)activeAccess->previous=&scope->next;
        activeAccess=scope;
    }
    access_unlock();
    if(conflict) {
        xl6ForeignContextCallLeaveV1(&scope->execution);
        xrtValueRelease(held);xrtSetErrorKind(XERR_STATE);return false;
    }
    if(mode==XL6_NATIVE_HANDLE_ACCESS_TAKE_V1) {
        void* taken=NULL;
        if(!xrtValueTakeHandle(value,&taken) || !taken) {
            xerror* error=xrtTakeError();
            (void)xl6NativeHandleAccessEndV1(scope);xrtClearError();xrtSetErrorTake(error);return false;
        }
        scope->raw=taken;
    }
    return true;
}

bool xl6NativeHandleAccessEndV1(XL6_NativeHandleAccessV1* scope)
{
    if(!scope || (uintptr_t)scope % _Alignof(XL6_NativeHandleAccessV1) || scope->self!=scope ||
        !scope->value || !scope->previous || !xl6ForeignContextCallContainsV1(&scope->execution)) {
        xrtSetErrorKind(XERR_STATE);return false;
    }
    access_lock();
    *scope->previous=scope->next;
    if(scope->next)scope->next->previous=scope->previous;
    scope->previous=NULL;scope->next=NULL;
    access_unlock();
    xvalue* value=scope->value;
    xl6ForeignContextCallLeaveV1(&scope->execution);
    *scope=(XL6_NativeHandleAccessV1){0};
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))abort();
    xrtValueRelease(value);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(!result.ok)xrtSetErrorTake(result.error);
    return result.ok;
}
