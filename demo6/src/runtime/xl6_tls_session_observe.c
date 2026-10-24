#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"

/* Called only while the session entry is held. A parsed/chain-checked
 * Certificate is not yet an authenticated peer before Finished succeeds. */
bool xl6TlsSessionPeerReadyInternalV1(const XL6_TlsSessionV1* self)
{
    xtlsstate state=xrtTlsSessionState(self->native);
    if(state==XTLS_STATE_FAILED){xl6TlsFailV1(XERR_STATE,"cannot inspect the authenticated peer of a failed TLS session");return false;}
    return state==XTLS_STATE_READY||state==XTLS_STATE_CLOSING||state==XTLS_STATE_CLOSED;
}
int64_t xl6TlsSessionPeerCertificateCountV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};int64_t result=0;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return 0;
    if(xl6TlsSessionPeerReadyInternalV1(self)&&self->role->certificateCount){
        size_t count=self->role->certificateCount(self->native);
        if(count>INT64_MAX)xl6TlsFailV1(XERR_RANGE,"TLS peer certificate count exceeds the language range");
        else result=(int64_t)count;
    }
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
int64_t xl6TlsSessionKeyUpdateV1(void* raw,bool requestPeer)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};xtlsresult result=XTLS_ERROR;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return result;
    if(!self->role->keyUpdate)xl6TlsFailV1(XERR_UNSUPPORTED,"TLS session has no role-specific key update operation");
    else result=self->role->keyUpdate(self->native,requestPeer?XTLS_KEY_UPDATE_REQUESTED:XTLS_KEY_UPDATE_NOT_REQUESTED);
    xl6TlsOutcomeV1(result!=XTLS_ERROR);xl6TlsSessionLeaveInternalV1(&scope);return result;
}
XL6_ProductBoolI64I64V1 xl6TlsSessionPeerAlertV1(void* raw)
{
    XL6_ProductBoolI64I64V1 result={false,0,0};XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return result;
    xtlsalertlevel level=0;xtlsalert alert=0;
    if(xrtTlsSessionPeerAlert(self->native,&level,&alert)){
        result.e0=true;result.e1=(int64_t)level;result.e2=(int64_t)alert;
    }
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
xvalue* xl6TlsSessionServerNameV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};xbytesview name={0};xvalue* result=NULL;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return NULL;
    if(!self->role->serverName)xl6TlsFailV1(XERR_STATE,"TLS received SNI is only available on a server session");
    else {
        bool available=self->role->serverName(self->native,&name);
        if(available||!xrtGetError())result=xrtValueBytes(name);
    }
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
uint64_t xl6TlsSessionServerCookieV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};uint64 result=0;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return 0;
    if(!self->role->serverCookie)xl6TlsFailV1(XERR_STATE,"TLS selection cookie is only available on a server session");
    else xl6TlsOutcomeV1(self->role->serverCookie(self->native,&result));
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
