#include "xl6_udp_internal.h"
#include "xl6_concurrent_deadline.h"
static bool validate(int64_t size,int64_t mode,unsigned operation,void* cancel,bool required)
{
    if((required&&!cancel)||(operation==1&&(size<0||(uint64_t)size>SIZE_MAX))||
        (operation==2&&(mode<XNET_UDP_WAIT_OPEN||mode>XNET_UDP_WAIT_CLOSE))||
        (operation==5&&(size<1||size>256))){xrtSetErrorKind(required&&!cancel?XERR_ARGUMENT:XERR_RANGE);xl6UdpFailedV1();return false;}
    return true;
}
static int64_t waitFor(void* raw,int64_t argument,uint64_t due,void* cancel,bool required,bool writable)
{
    if(!validate(argument,argument,writable?1:2,cancel,required))return XNET_RESULT_ERROR;
    XL6_UdpScopeV1 scope={0};if(!xl6UdpEnterV1(raw,false,&scope))return XNET_RESULT_ERROR;
    bool ok=writable?xrtNetUdpWritable(scope.udp,(size_t)argument,xl6DeadlineRemainingMillisecondsV1(due),cancel):xrtNetUdpWait(scope.udp,(xnetudpwait)argument,xl6DeadlineRemainingMillisecondsV1(due),cancel);
    xl6ExecutionUdpLeaveInternalV1(&scope);if(ok)return XNET_RESULT_OK;
    xerrkind kind=xrtErrorKind(xrtGetError());
    if(kind==XERR_TIMEOUT||kind==XERR_CLOSED||kind==XERR_CANCELLED){
        xrtClearError();return kind==XERR_TIMEOUT?XNET_RESULT_TIMEOUT:kind==XERR_CLOSED?XNET_RESULT_CLOSED:XNET_RESULT_CANCELLED;}
    xl6UdpFailedV1();return XNET_RESULT_ERROR;
}
static void* receive(void* raw,int64_t capacity,uint64_t due,void* cancel,bool required,unsigned operation)
{
    if(!validate(capacity,0,operation,cancel,required))return NULL;
    XL6_UdpScopeV1 scope={0};if(!xl6UdpEnterV1(raw,false,&scope))return NULL;
    void* result=operation==3?(void*)xrtNetUdpReceiveWait(scope.udp,xl6DeadlineRemainingMillisecondsV1(due),cancel):operation==4?
        (void*)xrtNetUdpReceiveErrorWait(scope.udp,xl6DeadlineRemainingMillisecondsV1(due),cancel):(void*)xrtNetUdpReceiveBatchWait(scope.udp,(size_t)capacity,xl6DeadlineRemainingMillisecondsV1(due),cancel);
    xl6ExecutionUdpLeaveInternalV1(&scope);if(!result)xl6UdpFailedV1();return result;
}
#define WAIT(Name,Writable) \
int64_t xl6Udp##Name##V1(void* r,int64_t a,int64_t ms){XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}return waitFor(r,a,due,NULL,false,Writable);} \
int64_t xl6Udp##Name##CancelV1(void* r,void* c,int64_t a,int64_t ms){XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}return waitFor(r,a,due,c,true,Writable);} \
int64_t xl6Udp##Name##UntilV1(void* r,uint64_t due,int64_t a){return waitFor(r,a,due,NULL,false,Writable);} \
int64_t xl6Udp##Name##UntilCancelV1(void* r,uint64_t due,void* c,int64_t a){return waitFor(r,a,due,c,true,Writable);}
WAIT(Wait,false)
WAIT(WaitWritable,true)
#undef WAIT
#define RECEIVE(Name,Operation) \
void* xl6Udp##Name##V1(void* r,int64_t ms){XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return NULL;}return receive(r,0,due,NULL,false,Operation);} \
void* xl6Udp##Name##CancelV1(void* r,void* c,int64_t ms){XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return NULL;}return receive(r,0,due,c,true,Operation);} \
void* xl6Udp##Name##UntilV1(void* r,uint64_t due){return receive(r,0,due,NULL,false,Operation);} \
void* xl6Udp##Name##UntilCancelV1(void* r,uint64_t due,void* c){return receive(r,0,due,c,true,Operation);}
RECEIVE(Receive,3)
RECEIVE(ReceiveError,4)
#undef RECEIVE
void* xl6UdpReceiveBatchV1(void* r,int64_t cap,int64_t ms)
{XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return NULL;}return receive(r,cap,due,NULL,false,5);}
void* xl6UdpReceiveBatchCancelV1(void* r,void* c,int64_t cap,int64_t ms)
{XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(ms,&due)){xl6UdpFailedV1();return NULL;}return receive(r,cap,due,c,true,5);}
void* xl6UdpReceiveBatchUntilV1(void* r,uint64_t due,int64_t cap){return receive(r,cap,due,NULL,false,5);}
void* xl6UdpReceiveBatchUntilCancelV1(void* r,uint64_t due,void* c,int64_t cap){return receive(r,cap,due,c,true,5);}
