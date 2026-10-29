#include "xl6_x509_crl_critical_internal.h"
typedef struct CriticalCall {
    XL6_X509CrlCriticalV1* context;
    XL6_Closure* callback;
    bool failed;
    xerror* error;
} CriticalCall;
static xx509result critical(const xx509crl* crl,const xx509crlentry* entry,const xx509ext* extension,void* raw)
{
    CriticalCall* call=raw;XL6_X509CrlCriticalV1* context=call->context;
    if(call->failed)return X509_ERROR;
    context->entry=entry;context->extension=extension;context->version=crl->Version;context->active=true;
    XL6_ClosureScope lease={0};xl6_error_scope scope={0};bool accepted=false;
    bool began=xl6ErrorScopeBegin(&scope),ok=false;
    if(began) {
        const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();runtime->Enter(&lease,call->callback);
        ok=((bool (*)(void*,bool*))call->callback->invoke)(call->callback->environment,&accepted);
        if(!ok)xl6ErrorScopeCaptureFailure(&scope);
        runtime->Leave(&lease);
    }
    context->active=false;context->entry=NULL;context->extension=NULL;
    xl6_error_result result=began?xl6ErrorScopeEnd(&scope):(xl6_error_result){false,xrtTakeError()};
    if(!result.ok) {
        call->failed=true;call->error=result.error;
        xrtClearError();xrtSetErrorTake(xrtErrorRef(call->error));return X509_ERROR;
    }
    return accepted?X509_VALUE:X509_DONE;
}
void* xl6X509CrlValidateCallbackV1(void* crl,void* issuer,int64_t time,int64_t skew,int64_t flags,void* raw,XL6_Closure* callback)
{
    XL6_X509CrlCriticalV1* context=raw;xx509crlconfig config;
    if(!context || !callback || !callback->invoke || callback->call_parameter_count) {
        xl6X509FailV1(XERR_ARGUMENT,"CRL validation needs a nullary checked bool callback and context");return NULL;
    }
    if(!xl6X509CrlPolicyConfigV1(time,skew,(uint64_t)flags,critical,NULL,&config) ||
        !xl6X509PolicyRetainV1(&context->references))return NULL;
    uint64 owner=xrtThreadCurrentId(),expected=0;
    if(!owner || !xrtAtomic64CompareExchange(&context->owner,&expected,owner,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        xl6X509FailV1(XERR_STATE,"CRL critical context is already in use");xl6X509CrlCriticalDestroyV1(context);return NULL;
    }
    CriticalCall call={.context=context,.callback=callback};config.UserData=&call;
    void* output=xl6X509ValidatedCrlCreateV1(crl,issuer,&config);
    context->active=false;context->entry=NULL;context->extension=NULL;
    xrtAtomic64Store(&context->owner,0,XMEMORY_RELEASE);xl6X509CrlCriticalDestroyV1(context);
    if(call.failed) {
        xl6X509ValidatedCrlDestroyV1(output);xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,call.error});return NULL;
    }
    return output;
}
