#include "xl6_tls_ticket_worker.h"
#include "xl6_tls_ticket_result.h"
void xl6TlsStreamIssueNewTicketV1(XL6_NativeClassResultContextV1 context,void* receiver,xvalue** output)
{
    if(!xl6TlsTicketResultRecipeV1(context)){xl6TlsTransportFailedV1();return;}
    xl6TlsTicketClassSyncV1(context,receiver,xl6TlsTicketNewCommandV1Get(),(xbytesview){0},0,output);
}
void xl6TlsStreamIssueTicketV1(XL6_NativeClassResultContextV1 context,void* receiver,const xvalue* ticket,int64_t lifetime,int64_t size,xvalue** output)
{
    xbytesview view={0};
    if(!xl6TlsTicketResultRecipeV1(context)||!xl6TlsTicketServerInputV1(ticket,lifetime,size,&view)){
        xl6TlsTransportFailedV1();return;
    }
    xl6TlsTicketClassSyncV1(context,receiver,xl6TlsTicketIssueCommandV1Get(),view,(uint32_t)lifetime,output);
}
