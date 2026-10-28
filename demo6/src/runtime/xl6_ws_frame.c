#include "xl6_ws_internal.h"

void* xl6WsFrameConfigCreateV2(uint64_t maxPayload,int64_t mask,int64_t rsv,int64_t opcodes)
{
    xwsframeconfig c;xrtWsFrameConfigInit(&c);uint16 o,r;
    if(mask<XWS_MASK_ANY||mask>XWS_MASK_FORBIDDEN||!xl6WsU16(opcodes,&o)||!xl6WsU16(rsv,&r)||
       (r&~(XWS_FRAME_RSV1|XWS_FRAME_RSV2|XWS_FRAME_RSV3))||maxPayload>XWS_FRAME_PAYLOAD_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    c.MaxPayload=maxPayload;c.Mask=(xwsmaskpolicy)mask;c.AllowedRsv=r;c.AllowedOpcodes=o;
    xwsframeconfig* result=xrtMalloc(sizeof(*result));if(result)*result=c;return result;
}
void xl6WsFrameConfigDestroyV2(void* raw){xrtFree(raw);}
uint64_t xl6WsFrameConfigMaxPayloadV2(void* raw){return ((xwsframeconfig*)raw)->MaxPayload;}
int64_t xl6WsFrameConfigMaskV2(void* raw){return ((xwsframeconfig*)raw)->Mask;}
int64_t xl6WsFrameConfigAllowedRsvV2(void* raw){return ((xwsframeconfig*)raw)->AllowedRsv;}
int64_t xl6WsFrameConfigAllowedOpcodesV2(void* raw){return ((xwsframeconfig*)raw)->AllowedOpcodes;}
static void config(xwsframeconfig* value,const void* raw)
{if(raw)*value=*(const xwsframeconfig*)raw;else{xrtWsFrameConfigInit(value);value->MaxPayload=1048576;}}
static void* parse(xbytesview input,const void* options)
{
    XL6_WsFrame value={0};xwsframeerrorinfo error={0};config(&value.config,options);
    xwsframestatus status=xrtWsFrameParse(input,&value.value,&value.config,&error);
    if(status==XWS_FRAME_MORE)return NULL;
    if(status!=XWS_FRAME_READY){xl6WsError("websocket-frame",error.Code,error.Offset);return NULL;}
    XL6_WsFrame* result=xrtMalloc(sizeof(*result));if(result)*result=value;return result;
}
void* xl6WsFrameParseTextV2(xstrview source,void* options,int64_t size)
{xbytesview input=xl6WsAsBytes(source);return xl6BufferSelect(&input,size)?parse(input,options):NULL;}
void* xl6WsFrameParseSpanV2(xbytesview input,void* options,int64_t size)
{return xl6BufferSelect(&input,size)?parse(input,options):NULL;}
void* xl6WsFrameParseBytesV2(xvalue* source,void* options,int64_t size)
{xbytesview input;return xl6BufferBytesInput(source,size,&input)?parse(input,options):NULL;}
void* xl6WsFrameParseBufferV2(void* source,void* options,int64_t size)
{xbytesview input;return xl6BufferInput(source,size,&input)?parse(input,options):NULL;}
void* xl6WsFrameMakeV2(int64_t opcode,uint64_t payload,void* options,int64_t flags,int64_t mask)
{
    XL6_WsFrame value={0};config(&value.config,options);xrtWsFrameInit(&value.value);
    if(opcode<0||opcode>UINT8_MAX||flags<0||(uint64_t)flags>UINT32_MAX||!xl6WsMask(mask,value.value.Mask)){
        xrtSetErrorKind(XERR_RANGE);return NULL;}
    value.value.Opcode=(uint8)opcode;value.value.PayloadSize=payload;value.value.Flags=(uint32)flags;
    size_t size=0;if(!xrtWsFrameWrite(&value.value,&value.config,NULL,0,&size))return NULL;
    value.value.HeadSize=size;XL6_WsFrame* result=xrtMalloc(sizeof(*result));if(result)*result=value;return result;
}
void* xl6WsFrameMakeDefaultV2(int64_t opcode,uint64_t payload,int64_t flags,int64_t mask)
{return xl6WsFrameMakeV2(opcode,payload,NULL,flags,mask);}
void* xl6WsFrameParseDefaultTextV2(xstrview source,int64_t size)
{return xl6WsFrameParseTextV2(source,NULL,size);}
void* xl6WsFrameParseDefaultSpanV2(xbytesview source,int64_t size)
{return xl6WsFrameParseSpanV2(source,NULL,size);}
void* xl6WsFrameParseDefaultBytesV2(xvalue* source,int64_t size)
{return xl6WsFrameParseBytesV2(source,NULL,size);}
void* xl6WsFrameParseDefaultBufferV2(void* source,int64_t size)
{return xl6WsFrameParseBufferV2(source,NULL,size);}
void xl6WsFrameDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6WsFrameOpcodeV2(void* raw){return ((XL6_WsFrame*)raw)->value.Opcode;}
uint64_t xl6WsFramePayloadSizeV2(void* raw){return ((XL6_WsFrame*)raw)->value.PayloadSize;}
int64_t xl6WsFrameFlagsV2(void* raw){return ((XL6_WsFrame*)raw)->value.Flags;}
int64_t xl6WsFrameMaskV2(void* raw)
{const uint8* m=((XL6_WsFrame*)raw)->value.Mask;return ((uint64)m[0]<<24)|((uint64)m[1]<<16)|((uint64)m[2]<<8)|m[3];}
int64_t xl6WsFrameHeaderSizeV2(void* raw){return (int64_t)((XL6_WsFrame*)raw)->value.HeadSize;}
bool xl6WsFrameFinalV2(void* raw){return (((XL6_WsFrame*)raw)->value.Flags&XWS_FRAME_FIN)!=0;}
bool xl6WsFrameMaskedV2(void* raw){return (((XL6_WsFrame*)raw)->value.Flags&XWS_FRAME_MASKED)!=0;}
static bool frameWire(const XL6_WsFrame* self,uint8 wire[XWS_FRAME_HEAD_MAX],size_t* size)
{return xrtWsFrameWrite(&self->value,&self->config,wire,XWS_FRAME_HEAD_MAX,size);}
int64_t xl6WsFrameWriteIntoV2(void* raw,void* output)
{uint8 wire[XWS_FRAME_HEAD_MAX];size_t size=0;xbuffer* buffer=xl6BufferChecked(output);return buffer&&frameWire(raw,wire,&size)&&xl6WsAppend(buffer,(xbytesview){wire,size})?(int64_t)size:0;}
xvalue* xl6WsFrameToBytesV2(void* raw)
{uint8 wire[XWS_FRAME_HEAD_MAX];size_t size=0;return frameWire(raw,wire,&size)?xrtValueBytes((xbytesview){wire,size}):NULL;}
void xl6WsFrameApplyMaskV2(void* raw,int64_t mask,uint64_t offset,int64_t size)
{xbuffer* data=xl6BufferChecked(raw);uint8 m[4];if(!data||!xl6WsMask(mask,m))return;xbytesview view=xrtBufferView(data);if(xl6BufferSelect(&view,size))(void)xrtWsMask(data->Data,view.Size,m,offset);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsFrameConfigHandlePolicyV2Get,xl6WsFrameConfigDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsFrameHandlePolicyV2Get,xl6WsFrameDestroyV2)
