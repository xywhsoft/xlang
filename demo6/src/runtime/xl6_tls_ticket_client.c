#include "xl6_tls_ticket_worker.h"
#include "xl6_tls_ticket_result.h"
static XL6_TaskOutcome take(xtlsstream* stream,XL6_NativeClassResultContextV1 context,XL6_CodeLease* lease,
    xcancel* cancel,xbytesview input,uint32_t lifetime,XL6_TaskResult* output)
{
    (void)input;(void)lifetime;
    xvalue** box=xrtCalloc(1,sizeof(*box));if(!box)return XL6_TASK_FAILED;
    *output=(XL6_TaskResult){box,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE),NULL};
    XL6_TlsTicketResultV1* record=NULL;*box=xl6TlsTicketResultPrepareV1(context,lease,true,&record);
    if(!*box)return XL6_TASK_FAILED;
    if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    xtlsresume* owned=xrtTlsClientTakeResume(xrtTlsStreamSession(stream));
    bool ok=!xrtGetError()&&xl6TlsTicketResultSetV1(record,owned?XTLS_OK:XTLS_AGAIN,&owned);
    xrtTlsResumeRelease(owned);return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
static const XL6_TlsTicketCommandV1 command={XTLS_CLIENT,true,XL6_FUTURE_PAYLOAD_XVALUE,take};
const XL6_TlsTicketCommandV1* xl6TlsTicketTakeCommandV1Get(void){return &command;}
xfuture* xl6TlsStreamTryTakeTicketAsyncV1(XL6_NativeClassResultContextV1 context,void* receiver)
{
    xfuture* result=xl6TlsTicketResultRecipeV1(context)?xl6TlsTicketSubmitV1(&context,receiver,&command,(xbytesview){0},0):NULL;
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
