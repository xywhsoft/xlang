#include "xl6_ws_upgrade_internal.h"
#include "xl6_native_class_callback.h"
static bool selectResponse(const xwsdeflate* offer,xwsdeflate* response,ptr raw)
{
    XL6_WsDeflate* copy=xrtMalloc(sizeof(*copy));xvalue* result=NULL;
    if(!copy){xl6ForeignCallbackFail();return false;}
    copy->value=*offer;void* input=copy;
    bool ok=xl6NativeClassCallbackInvokeTakeV1(raw,&input,&result);
    if(input)xl6WsDeflateOfferDestroyV2(input);
    if(!ok){xl6ForeignCallbackFail();return false;}
    if(!result)return false;
    void* value=NULL;
    ok=xrtValueGetHandle(result,&value,NULL,NULL)&&value;
    if(ok){
        xwsdeflate selected=((XL6_WsDeflate*)value)->value;
        ok=xrtWsDeflateResponseCheck(offer,&selected);
        if(ok)*response=selected;
    }
    xrtValueRelease(result);
    if(!ok)xl6ForeignCallbackFail();
    return ok;
}
void* xl6WsUpgradeServerSelectedV2(XL6_NativeClassCallbackContextV1 context,
    void* request,void* options,XL6_Closure* callback)
{
    if(context.argument.field.policy!=xl6WsDeflateOfferHandlePolicyV2Get()||
       context.result.policy!=xl6WsDeflateResponseHandlePolicyV2Get()){
        xrtSetErrorKind(XERR_TYPE);return NULL;
    }
    XL6_Closure* driver=xl6NativeClassCallbackCreateV1(context,callback);if(!driver)return NULL;
    xwsupgradeserverconfig value=((XL6_WsServerOptions*)options)->value;
    value.AcceptDeflate=selectResponse;value.DeflateData=driver;
    void* result=xl6WsUpgradeServerFromConfigV2(request,&value);
    xl6ClosureRuntimeV1Get()->Release(driver);return result;
}
