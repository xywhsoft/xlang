#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"
#include "xl6_net_buffer_internal.h"
int64_t xl6TlsSessionFeedBufferV1(void* raw,void* input)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 session={0};XL6_NetBufferV1* buffer=input;
    XL6_NetBufferGuardV1 guard={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&session))return XTLS_ERROR;
    if(!xl6NetBufferEnterV1(buffer,&guard)){xl6TlsSessionLeaveInternalV1(&session);return XTLS_ERROR;}
    xtlsresult result=XTLS_ERROR;
    if(buffer->faulted)xl6TlsFailV1(XERR_STATE,"cannot feed a faulted network buffer to TLS");
    else result=xrtTlsSessionFeedBuffer(self->native,&buffer->native);
    xl6TlsOutcomeV1(result!=XTLS_ERROR);xl6NetBufferLeaveV1(&guard);
    xl6TlsSessionLeaveInternalV1(&session);return result;
}
