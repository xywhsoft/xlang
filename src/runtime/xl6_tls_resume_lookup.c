#include "xl6_tls_role_context_internal.h"
#include "xl6_tls_resume_request.h"
#include "xl6_native_class_callback.h"
static void* ref(void* raw){return xl6ClosureRuntimeV1Get()->Retain(raw);}
static void drop(void* raw){xl6ClosureRuntimeV1Get()->Release(raw);}
static xrtownershipref ownership(const void* raw){return xl6ClosureRuntimeV1Get()->Ownership(raw);}
static bool lookup(ptr raw,const xtlsserverresumerequest* request,xtlsresume** output)
{
    if(!output||*output||!request){xrtSetErrorKind(XERR_ARGUMENT);xl6ForeignCallbackFail();return false;}
    void* input=xl6TlsResumeRequestCopyV1(request);xvalue* result=NULL;
    bool ok=input&&xl6NativeClassCallbackInvokeTakeV1(raw,&input,&result);
    if(input)xl6TlsResumeRequestDestroyV1(input);
    if(ok&&result){
        void* value=NULL;ok=xrtValueGetHandle(result,&value,NULL,NULL)&&value;
        /* Take a real reference while the temporary language result is still
         * alive. The native owned callback consumes it on every return path. */
        if(ok){*output=xrtTlsResumeRetain(value);ok=*output!=NULL;}
    }
    xrtValueRelease(result);
    if(!ok||xrtGetError()){xl6ForeignCallbackFail();return false;}
    return true; /* true + NULL is an intentional cache miss, not a throw. */
}
static const XL6_TlsResumeLookupOpsV1 operations={ref,drop,ownership,lookup};
void* xl6TlsServerContextWithResumeLookupV1(XL6_NativeClassCallbackContextV1 context,void* raw,XL6_Closure* callback,uint64_t tolerance)
{
    if(xrtGetError())return NULL;
    const XL6_TlsServerContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS server context");return NULL;}
    if(tolerance>UINT32_MAX){xl6TlsFailV1(XERR_RANGE,"TLS resume age tolerance exceeds uint32 milliseconds");return NULL;}
    if(context.argument.field.policy!=xl6TlsResumeRequestHandlePolicyV1Get()||context.result.policy!=xl6TlsResumeHandlePolicyV1Get()){
        xl6TlsFailV1(XERR_TYPE,"TLS resume lookup requires its exact request and credential policies");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    XL6_Closure* driver=xl6NativeClassCallbackCreateV1(context,callback);if(!driver){xrtFree(protocols);return NULL;}
    void* result=xl6TlsServerContextFromCallbacksV1(self->parameters,self->identity,protocols,self->requireProtocol,
        self->selector,(XL6_TlsResumeLookupV1){driver,&operations},(uint32)tolerance);
    drop(driver);return result;
}
