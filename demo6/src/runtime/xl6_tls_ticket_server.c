#include "xl6_tls_ticket_worker.h"
#include "xl6_tls_ticket_result.h"
static XL6_TaskOutcome issue(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output,bool random)
{
    xvalue** box=xrtCalloc(1,sizeof(*box));if(!box)return XL6_TASK_FAILED;
    *output=(XL6_TaskResult){box,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE),NULL};
    XL6_TlsTicketResultV1* record=NULL;*box=xl6TlsTicketResultPrepareV1(context,lease,true,&record);
    if(!*box)return XL6_TASK_FAILED;
    if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    xtlsresume* owned=NULL;xtlssession* session=xrtTlsStreamSession(stream);
    xtlsresult status=random?xrtTlsServerTicketNew(session,&owned):xrtTlsServerTicket(session,input,lifetime,&owned);
    bool ok=status!=XTLS_ERROR&&!xrtGetError()&&xl6TlsTicketResultSetV1(record,status,&owned);
    xrtTlsResumeRelease(owned);
    if(ok&&status==XTLS_OK){
        xtlsresult flushed=xrtTlsStreamFlush(stream);
        /* Producer success is already committed. Flush AGAIN belongs to the
         * ordinary driver, NOT to a repeat issueTicket invocation. */
        ok=(flushed==XTLS_OK||flushed==XTLS_AGAIN)&&!xrtGetError();
    }
    return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
static XL6_TaskOutcome manual(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output)
{return issue(stream,context,lease,cancel,input,lifetime,output,false);}
static XL6_TaskOutcome automatic(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output)
{return issue(stream,context,lease,cancel,input,lifetime,output,true);}
static const XL6_TlsTicketCommandV1 manualCommand={XTLS_SERVER,true,XL6_FUTURE_PAYLOAD_XVALUE,manual};
static const XL6_TlsTicketCommandV1 autoCommand={XTLS_SERVER,true,XL6_FUTURE_PAYLOAD_XVALUE,automatic};
const XL6_TlsTicketCommandV1* xl6TlsTicketIssueCommandV1Get(void){return &manualCommand;}
const XL6_TlsTicketCommandV1* xl6TlsTicketNewCommandV1Get(void){return &autoCommand;}
bool xl6TlsTicketServerInputV1(const xvalue* ticket,int64_t lifetime,int64_t size,xbytesview* view)
{
    if(lifetime<0||(uint64_t)lifetime>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    return xl6BufferBytesInput(ticket,size,view);
}
xfuture* xl6TlsStreamIssueNewTicketAsyncV1(XL6_NativeClassResultContextV1 context,void* receiver)
{
    xfuture* result=xl6TlsTicketResultRecipeV1(context)?xl6TlsTicketSubmitV1(&context,receiver,&autoCommand,(xbytesview){0},0):NULL;
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
xfuture* xl6TlsStreamIssueTicketAsyncV1(XL6_NativeClassResultContextV1 context,void* receiver,const xvalue* ticket,int64_t lifetime,int64_t size)
{
    xbytesview view={0};xfuture* result=xl6TlsTicketResultRecipeV1(context)&&xl6TlsTicketServerInputV1(ticket,lifetime,size,&view)?
        xl6TlsTicketSubmitV1(&context,receiver,&manualCommand,view,(uint32_t)lifetime):NULL;
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
