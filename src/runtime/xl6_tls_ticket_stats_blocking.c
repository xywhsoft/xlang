#include "xl6_tls_ticket_worker.h"
int64_t xl6TlsStreamTicketCountV1(void* receiver,int64_t millis)
{return (int64_t)xl6TlsTicketScalarSyncV1(receiver,xl6TlsTicketCountCommandV1Get(),millis);}
uint64_t xl6TlsStreamDroppedTicketsV1(void* receiver,int64_t millis)
{return xl6TlsTicketScalarSyncV1(receiver,xl6TlsTicketDroppedCommandV1Get(),millis);}
