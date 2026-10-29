#include "xl6_udp_future_internal.h"
#include "xl6_future_callback_interrupt.h"
#include "xl6_concurrent_deadline.h"
static xfuture* start(XL6_RuntimeServicesV1 services,XL6_UdpFutureKindV1 kind,
    const XL6_NativeClassResultContextV1* context,void* receiver,int64_t argument,
    uint64_t due,void* parent,bool requireParent)
{
    if(!services.executor||(due!=XRT_DEADLINE_NEVER&&!services.deadline)||(requireParent&&!parent)){
        xrtSetErrorKind(XERR_ARGUMENT);return xl6UdpFutureFailInternalV1(NULL,NULL);
    }
    XL6_UdpFutureCaptureV1* capture=xl6UdpFuturePrepareInternalV1(kind,receiver,argument,context);if(!capture)return NULL;
    if(!parent&&due==XRT_DEADLINE_NEVER)return xl6UdpFuturePublishInternalV1(services,capture,xl6UdpFutureSourceInternalV1(kind,receiver,argument));
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6UdpFutureFailInternalV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6UdpFutureSourceInternalV1(kind,receiver,argument);
    if(!source&&!stopped)return xl6UdpFutureFailInternalV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6UdpFutureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
#define RECEIVE(Name,Kind) \
xfuture* xl6Udp##Name##AsyncCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,void* p){return start(s,Kind,&c,r,0,XRT_DEADLINE_NEVER,p,true);} \
xfuture* xl6Udp##Name##AsyncUntilV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,uint64_t d){return start(s,Kind,&c,r,0,d,NULL,false);} \
xfuture* xl6Udp##Name##AsyncUntilCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,uint64_t d,void* p){return start(s,Kind,&c,r,0,d,p,true);} \
xfuture* xl6Udp##Name##AsyncTimeoutV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,int64_t ms){xdeadline d;if(!xl6ConcurrentDeadline(ms,&d))return xl6UdpFutureFailInternalV1(NULL,NULL);return start(s,Kind,&c,r,0,d,NULL,false);} \
xfuture* xl6Udp##Name##AsyncTimeoutCancelV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,void* p,int64_t ms){xdeadline d;if(!xl6ConcurrentDeadline(ms,&d))return xl6UdpFutureFailInternalV1(NULL,NULL);return start(s,Kind,&c,r,0,d,p,true);}
RECEIVE(Receive,XL6_UDP_FUTURE_PACKET)
RECEIVE(ReceiveError,XL6_UDP_FUTURE_ERROR)
#undef RECEIVE
#define ARGUMENT(Name,Kind,Context,Pointer) \
xfuture* xl6Udp##Name##AsyncCancelV1(XL6_RuntimeServicesV1 s,Context void* r,void* p,int64_t a){return start(s,Kind,Pointer,r,a,XRT_DEADLINE_NEVER,p,true);} \
xfuture* xl6Udp##Name##AsyncUntilV1(XL6_RuntimeServicesV1 s,Context void* r,uint64_t d,int64_t a){return start(s,Kind,Pointer,r,a,d,NULL,false);} \
xfuture* xl6Udp##Name##AsyncUntilCancelV1(XL6_RuntimeServicesV1 s,Context void* r,uint64_t d,void* p,int64_t a){return start(s,Kind,Pointer,r,a,d,p,true);} \
xfuture* xl6Udp##Name##AsyncTimeoutV1(XL6_RuntimeServicesV1 s,Context void* r,int64_t a,int64_t ms){xdeadline d;if(!xl6ConcurrentDeadline(ms,&d))return xl6UdpFutureFailInternalV1(NULL,NULL);return start(s,Kind,Pointer,r,a,d,NULL,false);} \
xfuture* xl6Udp##Name##AsyncTimeoutCancelV1(XL6_RuntimeServicesV1 s,Context void* r,void* p,int64_t a,int64_t ms){xdeadline d;if(!xl6ConcurrentDeadline(ms,&d))return xl6UdpFutureFailInternalV1(NULL,NULL);return start(s,Kind,Pointer,r,a,d,p,true);}
#define CLASS_CONTEXT XL6_NativeClassResultContextV1 c,
ARGUMENT(ReceiveBatch,XL6_UDP_FUTURE_BATCH,CLASS_CONTEXT,&c)
ARGUMENT(Wait,XL6_UDP_FUTURE_WAIT,,NULL)
ARGUMENT(WaitWritable,XL6_UDP_FUTURE_WRITABLE,,NULL)
#undef CLASS_CONTEXT
#undef ARGUMENT
