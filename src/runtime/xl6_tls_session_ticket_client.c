#include "xl6_tls_session_internal.h"
#include "xl6_tls_ticket_result.h"
static bool client(XL6_TlsSessionV1* self,XL6_TlsSessionScopeV1* scope)
{
    if(!xl6TlsSessionEnterInternalV1(self,true,scope))return false;
    if(xrtTlsSessionRole(self->native)==XTLS_CLIENT)return true;
    xl6TlsFailV1(XERR_TYPE,"TLS ticket receive requires a client session");
    xl6TlsSessionLeaveInternalV1(scope);return false;
}
int64_t xl6TlsSessionTicketCountV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};int64_t result=0;
    if(!client(self,&scope))return 0;
    size_t count=xrtTlsClientResumeCount(self->native);
    if(count>INT64_MAX)xl6TlsFailV1(XERR_RANGE,"TLS ticket count exceeds the language range");else result=(int64_t)count;
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
uint64_t xl6TlsSessionDroppedTicketsV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};uint64_t result=0;
    if(!client(self,&scope))return 0;
    result=xrtTlsClientResumeDropped(self->native);xl6TlsSessionLeaveInternalV1(&scope);return result;
}
void xl6TlsSessionTryTakeTicketV1(XL6_NativeClassResultContextV1 context,void* raw,xvalue** output)
{
    if(!output||*output){xl6TlsFailV1(XERR_ARGUMENT,"TLS ticket output must be an empty owned slot");goto failed;}
    XL6_TlsTicketResultV1* record=NULL;
    xvalue* result=xl6TlsTicketResultPrepareV1(context,NULL,false,&record);if(!result)goto failed;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};xtlsresume* owned=NULL;
    bool ok=client(self,&scope);
    if(ok){
        owned=xrtTlsClientTakeResume(self->native);
        ok=!xrtGetError()&&xl6TlsTicketResultSetV1(record,owned?XTLS_OK:XTLS_AGAIN,&owned);
        xl6TlsSessionLeaveInternalV1(&scope);
    }
    xrtTlsResumeRelease(owned);
    if(ok){*output=result;return;}
    xrtValueRelease(result);
failed:
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
}
