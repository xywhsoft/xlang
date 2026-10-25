#include "xl6_tls_selector_internal.h"
#include "xl6_native_class_callback.h"

static void* ref(void* raw){return xl6ClosureRuntimeV1Get()->Retain(raw);}
static void drop(void* raw){xl6ClosureRuntimeV1Get()->Release(raw);}
static xrtownershipref ownership(const void* raw){return xl6ClosureRuntimeV1Get()->Ownership(raw);}
static bool selectIdentity(ptr raw,const xtlsserverrequest* request,xtlsserverchoice* choice)
{
    if(!request||!choice){xrtSetErrorKind(XERR_ARGUMENT);xl6ForeignCallbackFail();return false;}
    void* input=xl6TlsServerRequestCopyV1(request->ServerName,request->Protocols);xvalue* result=NULL;
    bool ok=input&&xl6NativeClassCallbackInvokeTakeV1(raw,&input,&result);
    if(input)xl6TlsServerRequestDestroyV1(input);
    if(!ok){xl6ForeignCallbackFail();return false;}
    if(!result)return false; /* An intentional nullable rejection, not a throw. */
    void* value=NULL;
    if(!xrtValueGetHandle(result,&value,NULL,NULL)||!value){xrtValueRelease(result);xl6ForeignCallbackFail();return false;}
    const XL6_TlsServerChoiceV1* selected=value;
    ok=xrtTlsServerChoiceSetIdentityV1(choice,selected->identity);
    if(ok){
        choice->Cookie=selected->cookie;
        if(selected->hasProtocolIndex)choice->Protocol=selected->protocolIndex==-1?XTLS_SERVER_PROTOCOL_NONE:(size_t)selected->protocolIndex;
    }
    xrtValueRelease(result);
    if(!ok||xrtGetError()){xl6ForeignCallbackFail();return false;}return true;
}
static const XL6_TlsSelectorOpsV1 operations={ref,drop,ownership,selectIdentity};
static XL6_Closure* createDriver(XL6_NativeClassCallbackContextV1 context,XL6_Closure* callback)
{
    if(context.argument.field.policy!=xl6TlsServerRequestHandlePolicyV1Get()||context.result.policy!=xl6TlsServerChoiceHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_TYPE);return NULL;}
    return xl6NativeClassCallbackCreateV1(context,callback);
}
void* xl6TlsServerContextSelectorCreateV1(XL6_NativeClassCallbackContextV1 context,void* parameters,XL6_Closure* callback)
{
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"dynamic TLS server context needs parameters");return NULL;}
    XL6_Closure* driver=createDriver(context,callback);if(!driver)return NULL;
    void* result=xl6TlsServerContextFromSelectorV1(parameters,NULL,NULL,false,(XL6_TlsSelectorV1){driver,&operations});
    drop(driver);return result;
}
void* xl6TlsServerContextWithSelectorV1(XL6_NativeClassCallbackContextV1 context,void* raw,XL6_Closure* callback)
{
    if(xrtGetError())return NULL;
    const XL6_TlsServerContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS server context");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    XL6_Closure* driver=createDriver(context,callback);if(!driver){xrtFree(protocols);return NULL;}
    void* result=xl6TlsServerContextFromSelectorV1(self->parameters,self->identity,protocols,self->requireProtocol,(XL6_TlsSelectorV1){driver,&operations});
    drop(driver);return result;
}
