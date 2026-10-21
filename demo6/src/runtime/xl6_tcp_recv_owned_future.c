#include "xl6_tcp_internal.h"
#include "xl6_tcp_recv_owned_internal.h"
/* The class already owns a legitimate empty NetBytes, not a fake pointer or
 * borrowed native result. Only a resident callback installs the actual owned
 * native bytes; no generated descriptor/name/callback escapes its module. */
static bool mapBytes(const xvalue* object,const xvalue* expected,xstrview field,
    const xfutureresult* input,xvalue** output)
{
    if(!input||!output||xrtValueObjectGet(object,field)!=expected){xrtSetErrorKind(XERR_STATE);return false;}
    if(input->State==XFUTURE_RESOLVED){
        if(!input->Value){xrtSetErrorKind(XERR_STATE);return false;}
        void* owned=xrtNetBytesRef(input->Value);if(!owned)return false;
        xvalue* handle=xl6NativeHandleTakeV1(&owned,xl6NetBytesHandlePolicyV1Get());if(!handle)return false;
        bool ok=xrtValueObjectSet((xvalue*)object,field,handle);xrtValueRelease(handle);if(!ok)return false;
    }else if(input->State!=XFUTURE_CLOSED){xrtSetErrorKind(XERR_STATE);return false;}
    *output=xrtValueRetain(object);return *output!=NULL;
}
XL6_TcpClassCaptureV1* xl6TcpRecvOwnedPrepareInternalV1(XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,int64_t maximum,size_t* bytes)
{
    if(!bytes||!xl6TcpPositiveSizeV1(maximum,bytes)){
        if(!bytes)xrtSetErrorKind(XERR_ARGUMENT);
        (void)xl6TcpClassCaptureFailInternalV1(NULL,NULL);return NULL;
    }
    void* prepared=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpValueClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6NetBytesHandlePolicyV1Get(),mapBytes,&prepared);
    if(capture&&xrtNetBytesView(prepared).Size){
        xrtSetErrorKind(XERR_STATE);(void)xl6TcpClassCaptureFailInternalV1(capture,NULL);return NULL;
    }
    return capture;
}
xfuture* xl6TcpRecvOwnedSourceInternalV1(void* receiver,size_t maximum)
{
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return NULL;
    xfuture* result=xrtNetStreamRecvAsync(scope.stream,maximum);xl6ExecutionTcpLeaveInternalV1(&scope);return result;
}
xfuture* xl6TcpStreamRecvOwnedStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,int64_t maximum)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    size_t bytes=0;XL6_TcpClassCaptureV1* capture=xl6TcpRecvOwnedPrepareInternalV1(context,receiver,object,maximum,&bytes);
    return capture?xl6TcpClassCapturePublishInternalV1(services,capture,xl6TcpRecvOwnedSourceInternalV1(receiver,bytes)):NULL;
}
