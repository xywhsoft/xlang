#include "xl6_tcp_receive_into_internal.h"
xfuture* xl6TcpStreamRecvIntoNetBufferAsyncV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* target,int64_t maximum)
{return xl6TcpReceiveIntoStartInternalV1(services,context,receiver,target,maximum,xl6NetBufferHandlePolicyV1Get());}
