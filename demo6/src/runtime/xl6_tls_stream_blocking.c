#include "xl6_tls_io_internal.h"
xvalue* xl6TlsStreamRecvV1(void* receiver,int64_t maximum,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvSyncV1(receiver,maximum,due,NULL);
}
xvalue* xl6TlsStreamRecvCancelV1(void* receiver,int64_t maximum,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvSyncV1(receiver,maximum,due,parent);
}
xvalue* xl6TlsStreamRecvUntilV1(void* receiver,int64_t maximum,uint64_t due)
{
    return xl6TlsRecvSyncV1(receiver,maximum,due,NULL);
}
xvalue* xl6TlsStreamRecvUntilCancelV1(void* receiver,int64_t maximum,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvSyncV1(receiver,maximum,due,parent);
}
bool xl6TlsStreamWaitV1(void* receiver,int64_t condition,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return false;}
    return xl6TlsWaitSyncV1(receiver,condition,due,NULL);
}
bool xl6TlsStreamWaitCancelV1(void* receiver,int64_t condition,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return false;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return false;}
    return xl6TlsWaitSyncV1(receiver,condition,due,parent);
}
bool xl6TlsStreamWaitUntilV1(void* receiver,int64_t condition,uint64_t due)
{
    return xl6TlsWaitSyncV1(receiver,condition,due,NULL);
}
bool xl6TlsStreamWaitUntilCancelV1(void* receiver,int64_t condition,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return false;}
    return xl6TlsWaitSyncV1(receiver,condition,due,parent);
}
int64_t xl6TlsStreamSendTextV1(void* receiver,xstrview input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendTextCancelV1(void* receiver,xstrview input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
int64_t xl6TlsStreamSendTextUntilV1(void* receiver,xstrview input,int64_t size,uint64_t due)
{
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendTextUntilCancelV1(void* receiver,xstrview input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
int64_t xl6TlsStreamSendBytesV1(void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendBytesCancelV1(void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
int64_t xl6TlsStreamSendBytesUntilV1(void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendBytesUntilCancelV1(void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
int64_t xl6TlsStreamSendBufferV1(void* receiver,void* input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendBufferCancelV1(void* receiver,void* input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
int64_t xl6TlsStreamSendBufferUntilV1(void* receiver,void* input,int64_t size,uint64_t due)
{
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,NULL);
}
int64_t xl6TlsStreamSendBufferUntilCancelV1(void* receiver,void* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsSendSyncV1(receiver,data,due,parent);
}
