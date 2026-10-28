#include "xl6_ws_internal.h"
bool xl6WsHandshakeKeyValidV2(xstrview key,int64_t size)
{return xl6TextSelectRaw(&key,size)&&xrtWsKeyValid(key);}
xstrview xl6WsHandshakeAcceptV2(xstrview key,int64_t size)
{char accept[XWS_ACCEPT_CAPACITY];return xl6TextSelectRaw(&key,size)&&xrtWsAccept(key,accept,sizeof(accept))?xl6TextCopy((xstrview){accept,XWS_ACCEPT_SIZE}):xl6TextFailure;}
bool xl6WsHandshakeAcceptValidV2(xstrview key,xstrview accept){return xrtWsAcceptValid(key,accept);}
bool xl6WsHandshakeProtocolsValidV2(xstrview protocols,int64_t size)
{return xl6TextSelectRaw(&protocols,size)&&xrtWsProtocolsValid(protocols);}
bool xl6WsHandshakeProtocolsHasV2(xstrview protocols,xstrview protocol){return xrtWsProtocolsHas(protocols,protocol);}
xstrview xl6WsHandshakeProtocolSelectV2(xstrview client,xstrview server)
{xstrview result={0};return xrtWsProtocolSelect(client,server,&result)?xl6TextCopy(result):xl6TextFailure;}
int64_t xl6WsHandshakeProtocolCountV2(xstrview protocols,int64_t size)
{
    if(!xl6TextSelectRaw(&protocols,size)||!xrtWsProtocolsValid(protocols))return 0;
    size_t offset=0,count=0;xstrview token;xhttpnext status;
    while((status=xrtWsProtocolNext(protocols,&offset,&token))==XHTTP_NEXT_ITEM)++count;
    if(status==XHTTP_NEXT_ERROR)return 0;
    return (int64_t)count;
}
xstrview xl6WsHandshakeProtocolAtV2(xstrview protocols,int64_t index,int64_t size)
{
    if(index<0){xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    if(!xl6TextSelectRaw(&protocols,size)||!xrtWsProtocolsValid(protocols))return xl6TextFailure;
    size_t offset=0;xstrview token;xhttpnext status;
    for(int64_t i=0;(status=xrtWsProtocolNext(protocols,&offset,&token))==XHTTP_NEXT_ITEM;++i)if(i==index)return xl6TextCopy(token);
    if(status!=XHTTP_NEXT_ERROR)xrtSetErrorKind(XERR_RANGE);
    return xl6TextFailure;
}
