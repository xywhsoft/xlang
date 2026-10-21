#include "xl6_tcp_receive_into_internal.h"
#include "xl6_concurrent_deadline.h"
#include "xl6_net_frame_internal.h"
_Static_assert(offsetof(XL6_NetLineFramerV1,input)==0&&offsetof(XL6_NetLengthFramerV1,input)==0,
    "Framer input projection must remain its first native field");
bool xl6TcpReceiveIntoPrepareInternalV1(void* raw,size_t* maximum)
{
    XL6_NetBufferV1* input=raw;XL6_NetBufferGuardV1 guard={0};
    if(!xl6NetBufferEnterV1(input,&guard))return false;
    bool ok=xl6NetBufferCanAppendV1(input,1);
    if(ok&&*maximum>input->limit-input->native.Size)*maximum=input->limit-input->native.Size;
    xl6NetBufferLeaveV1(&guard);return ok;
}
bool xl6TcpReceiveIntoAppendInternalV1(void* raw,xbytesview bytes)
{
    XL6_NetBufferV1* input=raw;XL6_NetBufferGuardV1 guard={0};
    if(!xl6NetBufferEnterV1(input,&guard))return false;
    bool ok=xl6NetBufferCanAppendV1(input,bytes.Size)&&xrtNetBufAppend(&input->native,bytes.Data,bytes.Size);
    /* Read data cannot be put back on a byte stream. A failed delivery must
     * not leave a framer accepting new input over a silently missing chunk. */
    if(!ok&&bytes.Size)input->faulted=true;
    xl6NetBufferLeaveV1(&guard);return ok;
}
static int64_t receive(void* receiver,void* target,int64_t maximum,int64_t millis)
{
    size_t count;xdeadline deadline;
    if(!target||!xl6TcpPositiveSizeV1(maximum,&count)||!xl6ConcurrentDeadline(millis,&deadline)){
        if(!target)xrtSetErrorKind(XERR_ARGUMENT);
        xl6TcpFailedV1();return 0;}
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return 0;
    if(!xl6TcpReceiveIntoPrepareInternalV1(target,&count)){
        xl6ExecutionTcpLeaveInternalV1(&scope);xl6TcpFailedV1();return 0;}
    xnetbytes* bytes=xrtNetStreamRecv(scope.stream,count,deadline,NULL);bool ok=false;int64_t result=0;
    if(bytes){xbytesview view=xrtNetBytesView(bytes);ok=xl6TcpReceiveIntoAppendInternalV1(target,view);result=(int64_t)view.Size;xrtNetBytesDestroy(bytes);}
    else if(xrtErrorKind(xrtGetError())==XERR_CLOSED){xrtClearError();ok=true;}
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!ok)xl6TcpFailedV1();return result;
}
int64_t xl6TcpStreamRecvIntoNetBufferV1(void* r,void* t,int64_t m,int64_t w){return receive(r,t,m,w);}
int64_t xl6TcpStreamRecvIntoLineV1(void* r,void* t,int64_t m,int64_t w){return receive(r,t,m,w);}
int64_t xl6TcpStreamRecvIntoLengthV1(void* r,void* t,int64_t m,int64_t w){return receive(r,t,m,w);}
