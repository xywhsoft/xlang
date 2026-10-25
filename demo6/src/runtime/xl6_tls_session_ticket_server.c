#include "xl6_tls_session_internal.h"
#include "xl6_tls_ticket_result.h"
#include "xl6_buffer_internal.h"
static void issue(XL6_NativeClassResultContextV1 context,void* raw,xbytesview ticket,uint32_t lifetime,bool random,xvalue** output)
{
    if(!output||*output){xl6TlsFailV1(XERR_ARGUMENT,"TLS ticket output must be an empty owned slot");goto failed;}
    XL6_TlsTicketResultV1* record=NULL;
    xvalue* result=xl6TlsTicketResultPrepareV1(context,NULL,false,&record);if(!result)goto failed;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};xtlsresume* owned=NULL;
    bool ok=xl6TlsSessionEnterInternalV1(self,true,&scope);
    if(ok){
        if(xrtTlsSessionRole(self->native)!=XTLS_SERVER)ok=xl6TlsFailV1(XERR_TYPE,"TLS ticket issue requires a server session");
        else{
            xtlsresult status=random?xrtTlsServerTicketNew(self->native,&owned):xrtTlsServerTicket(self->native,ticket,lifetime,&owned);
            ok=status!=XTLS_ERROR&&!xrtGetError()&&xl6TlsTicketResultSetV1(record,status,&owned);
        }
        xl6TlsSessionLeaveInternalV1(&scope);
    }
    xrtTlsResumeRelease(owned);
    if(ok){*output=result;return;}
    xrtValueRelease(result);
failed:
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
}
void xl6TlsSessionIssueNewTicketV1(XL6_NativeClassResultContextV1 context,void* raw,xvalue** output)
{issue(context,raw,(xbytesview){0},0,true,output);}
void xl6TlsSessionIssueTicketV1(XL6_NativeClassResultContextV1 context,void* raw,const xvalue* input,int64_t lifetime,int64_t size,xvalue** output)
{
    xbytesview ticket={0};
    if(lifetime<0||(uint64_t)lifetime>UINT32_MAX){xl6TlsFailV1(XERR_RANGE,"TLS ticket lifetime exceeds its native range");goto failed;}
    if(!xl6BufferBytesInput(input,size,&ticket))goto failed;
    issue(context,raw,ticket,(uint32_t)lifetime,false,output);return;
failed:
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
}
