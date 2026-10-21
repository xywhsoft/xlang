#include "xl6_tcp_server_internal.h"
#include "xl6_tcp_future_internal.h"
#include "xl6_future_callback_interrupt.h"
/* Arm before starting native Accept. Timeout/cancellation never abandons the
 * real source; the common exclusive callback owns terminal delivery/cleanup. */
static xfuture* acceptFuture(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,uint64_t due,void* parent,bool requireParent)
{
    if(!services.executor||(due!=XRT_DEADLINE_NEVER&&!services.deadline)||(requireParent&&!parent)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6TcpServerFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpServerHandlePolicyV1Get(),xl6ExecutionTcpServerPromoteAcceptV1,false,&carrier);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6ExecutionTcpServerAcceptSourceForV1(carrier,receiver);
    if(!source&&!stopped)return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TcpClassCaptureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TcpServerAcceptStartCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,void* r,const xvalue* v,void* cancel)
{return acceptFuture(s,c,r,v,XRT_DEADLINE_NEVER,cancel,true);}
xfuture* xl6TcpServerAcceptStartUntilV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,void* r,const xvalue* v,uint64_t due)
{return acceptFuture(s,c,r,v,due,NULL,false);}
xfuture* xl6TcpServerAcceptStartUntilCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassFieldContextV1 c,void* r,const xvalue* v,uint64_t due,void* cancel)
{return acceptFuture(s,c,r,v,due,cancel,true);}
