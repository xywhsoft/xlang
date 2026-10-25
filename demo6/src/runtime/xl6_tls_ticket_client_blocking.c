#include "xl6_tls_ticket_worker.h"
#include "xl6_tls_ticket_result.h"
void xl6TlsStreamTryTakeTicketV1(XL6_NativeClassResultContextV1 context,void* receiver,xvalue** output)
{
    if(!xl6TlsTicketResultRecipeV1(context)){xl6TlsTransportFailedV1();return;}
    xl6TlsTicketClassSyncV1(context,receiver,xl6TlsTicketTakeCommandV1Get(),(xbytesview){0},0,output);
}
