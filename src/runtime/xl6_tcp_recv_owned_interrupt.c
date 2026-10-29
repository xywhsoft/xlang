#include "xl6_tcp_internal.h"
#include "xl6_tcp_recv_owned_internal.h"
#include "xl6_future_callback_interrupt.h"
static xfuture* receive(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,int64_t maximum,uint64_t due,void* parent,bool requireParent)
{
    if(!services.executor||(due!=XL6_DEADLINE_NEVER&&!services.deadline)||(requireParent&&!parent)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;
    }
    size_t bytes=0;XL6_TcpClassCaptureV1* capture=xl6TcpRecvOwnedPrepareInternalV1(context,receiver,object,maximum,&bytes);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TcpRecvOwnedSourceInternalV1(receiver,bytes);
    if(!source&&!stopped)return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TcpClassCaptureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TcpStreamRecvOwnedStartCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,
    void* r,const xvalue* o,void* cancel,int64_t maximum)
{return receive(s,c,r,o,maximum,XL6_DEADLINE_NEVER,cancel,true);}
xfuture* xl6TcpStreamRecvOwnedStartUntilV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,
    void* r,const xvalue* o,uint64_t due,int64_t maximum)
{return receive(s,c,r,o,maximum,due,NULL,false);}
xfuture* xl6TcpStreamRecvOwnedStartUntilCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,
    void* r,const xvalue* o,uint64_t due,void* cancel,int64_t maximum)
{return receive(s,c,r,o,maximum,due,cancel,true);}
