#include "xl6_execution_udp_internal.h"
typedef XL6_ExecutionResourceV1 Resource;
xfuture* xl6ExecutionUdpWaitAsyncV1(Resource* r,int wait)
{
    /* Waiting for close/drain remains valid after an explicit Close request. */
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xfuture* result=xrtNetUdpWaitAsync(scope.udp,(xnetudpwait)wait);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xfuture* xl6ExecutionUdpWritableAsyncV1(Resource* r,size_t size)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return NULL;
    xfuture* result=xrtNetUdpWritableAsync(scope.udp,size);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xfuture* xl6ExecutionUdpReceiveAsyncV1(Resource* r)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xfuture* result=xrtNetUdpReceiveAsync(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xfuture* xl6ExecutionUdpReceiveBatchAsyncV1(Resource* r,size_t capacity)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xfuture* result=xrtNetUdpReceiveBatchAsync(scope.udp,capacity);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xfuture* xl6ExecutionUdpReceiveErrorAsyncV1(Resource* r)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xfuture* result=xrtNetUdpReceiveErrorAsync(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
