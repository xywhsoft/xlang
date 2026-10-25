#include "xl6_tls_ticket_worker.h"
static XL6_TaskOutcome stats(xtlsstream* stream,xcancel* cancel,XL6_TaskResult* output,bool dropped)
{
    uint64_t* box=xrtCalloc(1,sizeof(*box));if(!box)return XL6_TASK_FAILED;
    *output=(XL6_TaskResult){box,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE),NULL};
    if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    xtlssession* session=xrtTlsStreamSession(stream);
    if(dropped)*box=xrtTlsClientResumeDropped(session);
    else{
        size_t count=xrtTlsClientResumeCount(session);
        if(count>INT64_MAX){xrtSetErrorKind(XERR_RANGE);return XL6_TASK_FAILED;}
        *box=(uint64_t)count;
    }
    return xrtGetError()?XL6_TASK_FAILED:XL6_TASK_SUCCESS;
}
static XL6_TaskOutcome count(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output)
{(void)context;(void)lease;(void)input;(void)lifetime;return stats(stream,cancel,output,false);}
static XL6_TaskOutcome dropped(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output)
{(void)context;(void)lease;(void)input;(void)lifetime;return stats(stream,cancel,output,true);}
static const XL6_TlsTicketCommandV1 countCommand={XTLS_CLIENT,false,XL6_FUTURE_PAYLOAD_VALUE,count};
static const XL6_TlsTicketCommandV1 droppedCommand={XTLS_CLIENT,false,XL6_FUTURE_PAYLOAD_VALUE,dropped};
const XL6_TlsTicketCommandV1* xl6TlsTicketCountCommandV1Get(void){return &countCommand;}
const XL6_TlsTicketCommandV1* xl6TlsTicketDroppedCommandV1Get(void){return &droppedCommand;}
static xfuture* source(void* receiver,const XL6_TlsTicketCommandV1* command)
{
    xfuture* result=xl6TlsTicketSubmitV1(NULL,receiver,command,(xbytesview){0},0);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
xfuture* xl6TlsStreamTicketCountAsyncV1(void* receiver){return source(receiver,&countCommand);}
xfuture* xl6TlsStreamDroppedTicketsAsyncV1(void* receiver){return source(receiver,&droppedCommand);}
