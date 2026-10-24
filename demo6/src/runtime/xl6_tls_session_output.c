#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"
#include <string.h>
/* Copy a bounded span batch, then box it, THEN consume the native queue. All
 * allocations precede consumption. Never modify a value through a const view,
 * depend on bytes-constructor intern/hash behavior, or expose borrowed spans.
 * A maximum is an upper bound; a read/take can be short without losing data. */
static xvalue* takeQueued(XL6_TlsSessionV1* self,int64_t capacity,bool plain)
{
    xnetspan spans[16];size_t count=0;
    size_t spanCount=plain?xrtTlsSessionPlainSpans(self->native,spans,16):xrtTlsSessionSendSpans(self->native,spans,16);
    if(xrtGetError())return NULL;
    for(size_t i=0;i<spanCount&&count<(uint64_t)capacity;++i){
        uint64_t remaining=(uint64_t)capacity-count;size_t part=spans[i].Size;
        if(part>remaining)part=(size_t)remaining;
        if(part>SIZE_MAX-count){xl6TlsFailV1(XERR_RANGE,"TLS output size overflow");return NULL;}
        count+=part;
    }
    if(!count){
        if((plain?xrtTlsSessionPlainSize(self->native):xrtTlsSessionSendSize(self->native))!=0){
            xl6TlsFailV1(XERR_INTERNAL,"TLS queued bytes have no readable span");return NULL;}
        return xrtValueBytes((xbytesview){0});
    }
    bytes data=xrtMalloc(count);if(!data)return NULL;
    size_t copied=0;
    for(size_t i=0;i<spanCount&&copied<count;++i){
        size_t part=spans[i].Size;if(part>count-copied)part=count-copied;
        memcpy(data+copied,spans[i].Data,part);copied+=part;
    }
    xvalue* value=NULL;
    if(copied==count)value=xrtValueBytesTake(&data,count);
    else xl6TlsFailV1(XERR_INTERNAL,"TLS output violated its queued span count");
    if(data&&plain)xrtSecureZero(data,count);
    xrtFree(data);
    if(value&&!xl6TlsOutcomeV1(plain?xrtTlsSessionPlainConsume(self->native,count):xrtTlsSessionSendConsume(self->native,count))){
        xrtValueRelease(value);value=NULL;
    }
    return value;
}
static bool maximum(int64_t size)
{return size>0||xl6TlsFailV1(XERR_RANGE,"TLS output maximum must be positive");}
XL6_ProductI64BytesV1 xl6TlsSessionReadV1(void* raw,int64_t capacity)
{
    XL6_ProductI64BytesV1 result={XTLS_ERROR,NULL};if(!maximum(capacity))return result;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return result;
    xtlsstate state=xrtTlsSessionState(self->native);
    if(state==XTLS_STATE_FAILED){
        xl6TlsFailV1(XERR_STATE,"cannot read a failed TLS session");
    }else if(xrtTlsSessionPlainSize(self->native)){
        /* Native protocol state is the only plaintext producer. No 0-RTT or
         * pre-authentication delivery is implied by a future queueing mode. */
        if(state!=XTLS_STATE_READY&&state!=XTLS_STATE_CLOSING&&state!=XTLS_STATE_CLOSED)
            xl6TlsFailV1(XERR_STATE,"TLS plaintext is not authenticated yet");
        else {result.e1=takeQueued(self,capacity,true);if(result.e1)result.e0=XTLS_OK;}
    }else{
        uint8 empty=0;size_t read=0;
        xtlsresult status=xrtTlsSessionRead(self->native,&empty,1,&read);
        if(xl6TlsOutcomeV1(status!=XTLS_ERROR)&&!read){
            result.e1=xrtValueBytes((xbytesview){0});if(result.e1)result.e0=status;
        }else if(!xrtGetError())xl6TlsFailV1(XERR_INTERNAL,"empty TLS read unexpectedly consumed bytes");
    }
    xl6TlsSessionLeaveInternalV1(&scope);return result;
}
xvalue* xl6TlsSessionTakeCipherV1(void* raw,int64_t capacity)
{
    if(!maximum(capacity))return NULL;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return NULL;
    xvalue* value=takeQueued(self,capacity,false);
    xl6TlsSessionLeaveInternalV1(&scope);return value;
}
