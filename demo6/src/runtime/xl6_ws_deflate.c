#include "xl6_ws_internal.h"
typedef bool (*Write)(const xwsdeflate*,void*,size_t,size_t*);
static XL6_WsDeflate* copy(const xwsdeflate* value)
{XL6_WsDeflate* result=xrtMalloc(sizeof(*result));if(result)result->value=*value;return result;}
static void* create(int64_t flags,int64_t server,int64_t client,Write write)
{
    if(flags<0||(uint64_t)flags>UINT32_MAX||server<8||server>15||client<8||client>15){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xwsdeflate value;xrtWsDeflateInit(&value);value.Flags=(uint32)flags;
    value.ServerMaxWindowBits=(uint8)server;value.ClientMaxWindowBits=(uint8)client;
    size_t size=0;return write(&value,NULL,0,&size)?copy(&value):NULL;
}
typedef bool (*Parse)(const xwsextension*,xwsdeflate*);
static void* parse(xbytesview input,Parse parseValue)
{
    size_t offset=0;xwsextension extension,extra;
    xstrview text=xl6WsAsText(input);
    if(xrtWsExtensionNext(text,&offset,&extension)!=XHTTP_NEXT_ITEM){xl6WsChecked(false);return NULL;}
    if(xrtWsExtensionNext(text,&offset,&extra)!=XHTTP_NEXT_END){xrtSetErrorKind(XERR_PROTOCOL);return NULL;}
    if(!xrtWsDeflateIs(&extension)){xrtSetErrorKind(XERR_PROTOCOL);return NULL;}
    xwsdeflate value;return parseValue(&extension,&value)?copy(&value):NULL;
}
#define TYPE(Name,Writer,Parser) \
void* xl6##Name##CreateV2(int64_t flags,int64_t server,int64_t client){return create(flags,server,client,Writer);} \
void xl6##Name##DestroyV2(void* raw){xrtFree(raw);} \
void* xl6##Name##ParseTextV2(xstrview source,int64_t size){xbytesview input=xl6WsAsBytes(source);return xl6BufferSelect(&input,size)?parse(input,Parser):NULL;} \
void* xl6##Name##ParseSpanV2(xbytesview input,int64_t size){return xl6BufferSelect(&input,size)?parse(input,Parser):NULL;} \
void* xl6##Name##ParseBytesV2(xvalue* source,int64_t size){xbytesview input;return xl6BufferBytesInput(source,size,&input)?parse(input,Parser):NULL;} \
void* xl6##Name##ParseBufferV2(void* source,int64_t size){xbytesview input;return xl6BufferInput(source,size,&input)?parse(input,Parser):NULL;} \
int64_t xl6##Name##FlagsV2(void* raw){return ((XL6_WsDeflate*)raw)->value.Flags;} \
int64_t xl6##Name##ServerWindowBitsV2(void* raw){return ((XL6_WsDeflate*)raw)->value.ServerMaxWindowBits;} \
int64_t xl6##Name##ClientWindowBitsV2(void* raw){return ((XL6_WsDeflate*)raw)->value.ClientMaxWindowBits;} \
xstrview xl6##Name##ToStringV2(void* raw){char text[XWS_DEFLATE_MAX_SIZE];size_t size=0;return Writer(&((XL6_WsDeflate*)raw)->value,text,sizeof(text),&size)?xl6TextCopy((xstrview){text,size}):xl6TextFailure;} \
int64_t xl6##Name##WriteIntoV2(void* raw,void* target){char text[XWS_DEFLATE_MAX_SIZE];size_t size=0;xbuffer* out=xl6BufferChecked(target);return out&&Writer(&((XL6_WsDeflate*)raw)->value,text,sizeof(text),&size)&&xl6WsAppend(out,(xbytesview){(const uint8*)text,size})?(int64_t)size:0;} \
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6##Name##HandlePolicyV2Get,xl6##Name##DestroyV2)
TYPE(WsDeflateOffer,xrtWsDeflateOfferWrite,xrtWsDeflateOfferParse)
TYPE(WsDeflateResponse,xrtWsDeflateResponseWrite,xrtWsDeflateResponseParse)
#undef TYPE
void* xl6WsDeflateOfferAcceptV2(void* raw)
{xwsdeflate value;return xrtWsDeflateAccept(&((XL6_WsDeflate*)raw)->value,&value)?copy(&value):NULL;}
void xl6WsDeflateOfferCheckV2(void* raw,void* response)
{(void)xl6WsChecked(xrtWsDeflateResponseCheck(&((XL6_WsDeflate*)raw)->value,&((XL6_WsDeflate*)response)->value));}
void* xl6WsDeflateResponseDirectionV2(void* raw,int64_t role,bool sending)
{
    if(role<XWS_ROLE_CLIENT||role>XWS_ROLE_SERVER){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xwsdeflatedirection value;
    if(!xrtWsDeflateDirection(&((XL6_WsDeflate*)raw)->value,(xwsrole)role,sending,&value))return NULL;
    xwsdeflatedirection* result=xrtMalloc(sizeof(*result));if(result)*result=value;return result;
}
void xl6WsDeflateDirectionDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6WsDeflateDirectionWindowBitsV2(void* raw){return ((xwsdeflatedirection*)raw)->WindowBits;}
bool xl6WsDeflateDirectionNoContextTakeoverV2(void* raw){return ((xwsdeflatedirection*)raw)->NoContextTakeover;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsDeflateDirectionHandlePolicyV2Get,xl6WsDeflateDirectionDestroyV2)
