#include "xl6_ws_internal.h"
static void* closeCopy(uint16 code,xstrview text)
{
    if(text.Size>SIZE_MAX-sizeof(XL6_WsClose)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_WsClose* result=xrtMalloc(sizeof(*result)+text.Size);if(!result)return NULL;
    result->code=code;result->size=text.Size;if(text.Size)memcpy(result->text,text.Data,text.Size);return result;
}
void* xl6WsCloseMakeV2(int64_t code,xstrview reason,int64_t size)
{uint16 c;size_t count=0;if(!xl6WsU16(code,&c)||!xl6TextSelectRaw(&reason,size)||!xrtWsCloseWrite(c,reason,NULL,0,&count))return NULL;return closeCopy(c,reason);}
static void* closeParse(xbytesview input)
{xwsclose value={0};return xrtWsCloseParse(input,&value)?closeCopy(value.Code,value.Reason):NULL;}
void* xl6WsCloseParseTextV2(xstrview source,int64_t size)
{xbytesview input=xl6WsAsBytes(source);return xl6BufferSelect(&input,size)?closeParse(input):NULL;}
void* xl6WsCloseParseSpanV2(xbytesview input,int64_t size){return xl6BufferSelect(&input,size)?closeParse(input):NULL;}
void* xl6WsCloseParseBytesV2(xvalue* source,int64_t size){xbytesview input;return xl6BufferBytesInput(source,size,&input)?closeParse(input):NULL;}
void* xl6WsCloseParseBufferV2(void* source,int64_t size){xbytesview input;return xl6BufferInput(source,size,&input)?closeParse(input):NULL;}
bool xl6WsCloseCodeValidV2(int64_t code){return code>=0&&code<=UINT16_MAX&&xrtWsCloseCodeValid((uint16)code);}
void xl6WsCloseDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6WsCloseCodeV2(void* raw){return ((XL6_WsClose*)raw)->code;}
xstrview xl6WsCloseReasonV2(void* raw){XL6_WsClose* s=raw;return xl6WsText((xstrview){s->text,s->size});}
static bool closeWire(XL6_WsClose* self,uint8 wire[XWS_CLOSE_PAYLOAD_MAX],size_t* size)
{return xrtWsCloseWrite(self->code,(xstrview){self->text,self->size},wire,XWS_CLOSE_PAYLOAD_MAX,size);}
int64_t xl6WsCloseWriteIntoV2(void* raw,void* output)
{uint8 wire[XWS_CLOSE_PAYLOAD_MAX];size_t size=0;xbuffer* buffer=xl6BufferChecked(output);return buffer&&closeWire(raw,wire,&size)&&xl6WsAppend(buffer,(xbytesview){wire,size})?(int64_t)size:0;}
xvalue* xl6WsCloseToBytesV2(void* raw)
{uint8 wire[XWS_CLOSE_PAYLOAD_MAX];size_t size=0;return closeWire(raw,wire,&size)?xrtValueBytes((xbytesview){wire,size}):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsCloseHandlePolicyV2Get,xl6WsCloseDestroyV2)
