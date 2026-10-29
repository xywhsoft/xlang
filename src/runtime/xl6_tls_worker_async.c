#include "xl6_tls_data_internal.h"
static xfuture* checkedSource(void* receiver,int64_t argument,unsigned operation)
{
    xfuture* result=xl6TlsWorkerSourceV1(receiver,argument,operation);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
xfuture* xl6TlsStreamPeekAsyncV1(void* receiver,int64_t maximum)
{return checkedSource(receiver,maximum,XL6_TLS_WORKER_PEEK_V1);}
xfuture* xl6TlsStreamConsumeAsyncV1(void* receiver,int64_t count)
{return checkedSource(receiver,count,XL6_TLS_WORKER_CONSUME_V1);}
xfuture* xl6TlsStreamSendBoundAsyncV1(void* receiver,int64_t count)
{return checkedSource(receiver,count,XL6_TLS_WORKER_SEND_BOUND_V1);}
xfuture* xl6TlsStreamProtocolAsyncV1(void* receiver)
{return checkedSource(receiver,0,XL6_TLS_WORKER_PROTOCOL_V1);}
xfuture* xl6TlsStreamKeyUpdateAsyncV1(void* receiver,bool requestPeer)
{return checkedSource(receiver,requestPeer?1:0,XL6_TLS_WORKER_KEY_UPDATE_V1);}
