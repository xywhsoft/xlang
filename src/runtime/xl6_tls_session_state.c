#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"
#define QUERY(Type,Name,Expression) \
Type xl6TlsSession##Name##V1(void* raw) { \
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0}; \
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return 0; \
    Type result=(Expression);xl6TlsSessionLeaveInternalV1(&scope);return result; }
QUERY(int64_t,Role,(int64_t)xrtTlsSessionRole(self->native))
QUERY(int64_t,State,(int64_t)xrtTlsSessionState(self->native))
QUERY(int64_t,Wait,(int64_t)xrtTlsSessionWait(self->native))
QUERY(bool,Ready,xrtTlsSessionState(self->native)==XTLS_STATE_READY)
QUERY(uint64_t,PendingInput,(uint64_t)xrtTlsSessionFeedSize(self->native))
QUERY(uint64_t,PendingCipher,(uint64_t)xrtTlsSessionSendSize(self->native))
QUERY(uint64_t,PendingPlain,(uint64_t)xrtTlsSessionPlainSize(self->native))
#undef QUERY
bool xl6TlsSessionResumedV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};bool result=false;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return false;
    if(!self->role->resumed)xl6TlsFailV1(XERR_UNSUPPORTED,"TLS session has no certified resumption observer");
    else result=self->role->resumed(self->native);
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
static int64_t selection(void* raw,bool cipher)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return 0;
    int64_t value=cipher?(int64_t)xrtTlsSessionCipher(self->native):(int64_t)xrtTlsSessionVersion(self->native);
    if(!value&&!xrtGetError())xl6TlsFailV1(XERR_STATE,"TLS selection is not available yet");
    xl6TlsSessionLeaveInternalV1(&scope);return value;
}
int64_t xl6TlsSessionVersionV1(void* raw){return selection(raw,false);}
int64_t xl6TlsSessionCipherV1(void* raw){return selection(raw,true);}
static int64_t control(void* raw,unsigned operation)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return XTLS_ERROR;
    xtlsresult result=operation==0?self->role->drive(self->native):
        operation==1?xrtTlsSessionClose(self->native):xrtTlsSessionEof(self->native);
    xl6TlsOutcomeV1(result!=XTLS_ERROR);
    /* Select is borrowed only through the native first flight. On a terminal
     * handshake state no future Drive can call it again; drop captures while
     * the real Session entry gate still excludes graph observation. */
    xtlsstate state=xrtTlsSessionState(self->native);
    if(state==XTLS_STATE_READY||state==XTLS_STATE_FAILED){
        xl6TlsSelectorDropV1(&self->selector);xl6TlsResumeLookupDropV1(&self->resumeLookup);}
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
int64_t xl6TlsSessionDriveV1(void* raw){return control(raw,0);}
int64_t xl6TlsSessionCloseV1(void* raw){return control(raw,1);}
int64_t xl6TlsSessionEofV1(void* raw){return control(raw,2);}
xvalue* xl6TlsSessionProtocolV1(void* raw)
{
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};xbytesview view={0};xvalue* result=NULL;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return NULL;
    bool selected=xrtTlsSessionProtocol(self->native,&view);
    if(selected||!xrtGetError())result=xrtValueBytes(view);
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
