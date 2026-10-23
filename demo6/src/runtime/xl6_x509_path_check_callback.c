#include "xl6_x509_path_critical_internal.h"
typedef struct Dispatch {XL6_X509PathCriticalV1* context;XL6_Closure* callback;} Dispatch;
static xx509result critical(const xx509cert* certificate,const xx509ext* extension,size_t depth,void* raw)
{
    Dispatch* call=raw;XL6_X509PathCriticalV1* context=call->context;
    context->certificate=certificate;context->extension=extension;context->depth=depth;context->active=true;
    XL6_ClosureScope lease={0};xl6_error_scope scope={0};bool accepted=false;
    bool began=xl6ErrorScopeBegin(&scope);
    if(began) {
        const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();runtime->Enter(&lease,call->callback);
        bool ok=((bool (*)(void*,bool*))call->callback->invoke)(call->callback->environment,&accepted);
        if(!ok)xl6ErrorScopeCaptureFailure(&scope);
        runtime->Leave(&lease);
    }
    context->active=false;context->certificate=NULL;context->extension=NULL;context->depth=0;
    xl6_error_result result=began?xl6ErrorScopeEnd(&scope):(xl6_error_result){false,xrtTakeError()};
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return X509_ERROR;}
    return accepted?X509_VALUE:X509_DONE;
}
void* xl6X509PathCheckCallbackV1(XL6_NativeClassArrayContextV1 input,const xvalue* array,void* anchor,void* policy,void* raw,XL6_Closure* callback)
{
    XL6_X509PathCriticalV1* context=raw;
    if(!context || !callback || !callback->invoke || callback->call_parameter_count) {
        xl6X509FailV1(XERR_ARGUMENT,"path check needs a nullary checked bool dispatcher and context");return NULL;
    }
    if(!xl6X509PolicyRetainV1(&context->references))return NULL;
    uint64 owner=xrtThreadCurrentId(),expected=0;
    if(!owner || !xrtAtomic64CompareExchange(&context->owner,&expected,owner,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        xl6X509FailV1(XERR_STATE,"path callback context is already in use");xl6X509PathCriticalDestroyV1(context);return NULL;
    }
    Dispatch call={context,callback};void* result=xl6X509PathArrayRunV1(input,array,anchor,policy,critical,&call);
    context->active=false;context->certificate=NULL;context->extension=NULL;context->depth=0;
    xrtAtomic64Store(&context->owner,0,XMEMORY_RELEASE);xl6X509PathCriticalDestroyV1(context);return result;
}
