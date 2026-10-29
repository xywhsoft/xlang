#include "xl6_tls_session_internal.h"
#include "xl6_tls_session_abi.h"
#include "xl6_buffer_internal.h"
static int64_t feed(void* raw,xbytesview input,int64_t size)
{
    if(!xl6BufferSelect(&input,size))return XTLS_ERROR;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return XTLS_ERROR;
    xtlsresult result=xrtTlsSessionFeed(self->native,input.Data,input.Size);
    xl6TlsOutcomeV1(result!=XTLS_ERROR);xl6TlsSessionLeaveInternalV1(&scope);return result;
}
int64_t xl6TlsSessionFeedTextV1(void* raw,xstrview input,int64_t size)
{return feed(raw,(xbytesview){(const uint8*)input.Data,input.Size},size);}
int64_t xl6TlsSessionFeedBytesV1(void* raw,const xvalue* value,int64_t size)
{
    xbytesview input={0};if(!xl6BufferBytesInput(value,size,&input))return XTLS_ERROR;
    return feed(raw,input,-1);
}
static XL6_ProductI64U64V1 write(void* raw,xbytesview input,int64_t size)
{
    XL6_ProductI64U64V1 result={XTLS_ERROR,0};
    if(!xl6BufferSelect(&input,size))return result;
    XL6_TlsSessionV1* self=raw;XL6_TlsSessionScopeV1 scope={0};size_t written=0;
    if(!xl6TlsSessionEnterInternalV1(self,true,&scope))return result;
    result.e0=xrtTlsSessionWrite(self->native,input.Data,input.Size,&written);result.e1=(uint64_t)written;
    xl6TlsOutcomeV1(result.e0!=XTLS_ERROR);xl6TlsSessionLeaveInternalV1(&scope);return result;
}
XL6_ProductI64U64V1 xl6TlsSessionWriteTextV1(void* raw,xstrview input,int64_t size)
{return write(raw,(xbytesview){(const uint8*)input.Data,input.Size},size);}
XL6_ProductI64U64V1 xl6TlsSessionWriteBytesV1(void* raw,const xvalue* value,int64_t size)
{
    xbytesview input={0};if(!xl6BufferBytesInput(value,size,&input))return (XL6_ProductI64U64V1){XTLS_ERROR,0};
    return write(raw,input,-1);
}
