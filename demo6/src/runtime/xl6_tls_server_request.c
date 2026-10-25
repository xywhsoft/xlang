#include "xl6_tls_server_request.h"
#include <stdint.h>
#include <string.h>

struct XL6_TlsServerRequestV1 {
    xbytesview serverName;
    size_t protocolCount;
    xbytesview protocols[];
};

XL6_TlsServerRequestV1* xl6TlsServerRequestCopyV1(xbytesview name,xbytesview protocols)
{
    if(xrtGetError())return NULL;
    if((name.Size&&!name.Data)||(protocols.Size&&!protocols.Data)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    /* Check untrusted sizes before parsing or touching their data. The ALPN
     * cursor itself enforces the wire-vector grammar; no second parser here. */
    if(name.Size>SIZE_MAX-sizeof(XL6_TlsServerRequestV1)||
        protocols.Size>SIZE_MAX-sizeof(XL6_TlsServerRequestV1)-name.Size){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    xtlsprotocolcursor cursor={0},first={0};
    size_t count=0;
    if(protocols.Size){
        if(!xrtTlsProtocols(protocols,&cursor))return NULL;
        first=cursor;
        xbytesview item={0};xtlsitemresult result;
        while((result=xrtTlsProtocolsRead(&cursor,&item))==XTLS_ITEM_VALUE)++count;
        if(result!=XTLS_ITEM_DONE)return NULL;
    }
    size_t bytes=sizeof(XL6_TlsServerRequestV1)+name.Size+protocols.Size;
    if(count>(SIZE_MAX-bytes)/sizeof(xbytesview)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    XL6_TlsServerRequestV1* self=xrtMalloc(bytes+count*sizeof(xbytesview));
    if(!self)return NULL;
    self->protocolCount=count;
    uint8* data=(uint8*)(self->protocols+count);
    self->serverName=(xbytesview){name.Size?data:NULL,name.Size};
    if(name.Size)memcpy(data,name.Data,name.Size);
    data+=name.Size;
    if(protocols.Size){
        memcpy(data,protocols.Data,protocols.Size);
        /* Relocate the already validated native cursor into our copy. No
         * wire offsets/length prefixes are parsed again by the adapter. */
        cursor=first;
        cursor.Data.Data=data+(first.Data.Data-protocols.Data);
        for(size_t i=0;i<count;++i){
            if(xrtTlsProtocolsRead(&cursor,&self->protocols[i])!=XTLS_ITEM_VALUE){
                xrtFree(self);return NULL;
            }
        }
    }
    return self;
}

void xl6TlsServerRequestDestroyV1(void* raw){xrtFree(raw);}

static const XL6_TlsServerRequestV1* checked(const void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xrtSetErrorKind(XERR_ARGUMENT);
    return raw;
}
xvalue* xl6TlsServerRequestNameV1(const void* raw)
{
    const XL6_TlsServerRequestV1* self=checked(raw);
    return self?xrtValueBytes(self->serverName):NULL;
}
int64_t xl6TlsServerRequestProtocolCountV1(const void* raw)
{
    const XL6_TlsServerRequestV1* self=checked(raw);
    /* A validated ALPN list has a 16-bit byte length and nonempty names. */
    return self?(int64_t)self->protocolCount:0;
}
xvalue* xl6TlsServerRequestProtocolAtV1(const void* raw,int64_t index)
{
    const XL6_TlsServerRequestV1* self=checked(raw);
    if(!self)return NULL;
    if(index<0||(uint64_t)index>=self->protocolCount){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    return xrtValueBytes(self->protocols[(size_t)index]);
}
