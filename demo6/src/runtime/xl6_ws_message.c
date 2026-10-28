#include "xl6_ws_internal.h"
typedef struct State {xatomic32 gate;xwsmessagestate value;} State;
void* xl6WsMessageConfigCreateV2(int64_t limit,int64_t first,int64_t continuation,int64_t control,bool text)
{
    xwsmessageconfig config;xrtWsMessageConfigInitSafe(&config);
    if(limit< -1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(!xl6WsU16(first,&config.FirstRsv)||!xl6WsU16(continuation,&config.ContinuationRsv)||!xl6WsU16(control,&config.ControlRsv))return NULL;
    config.MaxSize=limit<0?SIZE_MAX:(size_t)limit;config.ValidateText=text;
    xwsmessagestate probe;if(!xrtWsMessageInit(&probe,&config))return NULL;
    xwsmessageconfig* result=xrtMalloc(sizeof(*result));if(result)*result=config;return result;
}
void xl6WsMessageConfigDestroyV2(void* raw){xrtFree(raw);}
void* xl6WsMessageStateCreateV2(void* raw)
{
    xwsmessageconfig config;if(raw)config=*(xwsmessageconfig*)raw;else xrtWsMessageConfigInitSafe(&config);
    xwsmessagestate initial;if(!xrtWsMessageInit(&initial,&config))return NULL;
    State* result=xrtCalloc(1,sizeof(*result));if(result)result->value=initial;return result;
}
void* xl6WsMessageStateCreateDefaultV2(void){return xl6WsMessageStateCreateV2(NULL);}
void xl6WsMessageStateDestroyV2(void* raw){xrtFree(raw);}
void* xl6WsMessageStateBeginFrameV2(void* raw,void* frame)
{
    State* s=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return NULL;
    xwsmessageinfo info={0};xwsmessageerrorinfo error={0};xwsmessagestate next=s->value;
    bool ok=xrtWsMessageFrameBegin(&next,&((XL6_WsFrame*)frame)->value,&info,&error);
    xwsmessageinfo* result=NULL;
    if(ok){result=xrtMalloc(sizeof(*result));if(result){*result=info;s->value=next;}}
    else{s->value=next;xl6WsError("websocket-message",error.Code,error.Offset);}
    xl6WsLeave(&guard);return result;
}
static void payload(State* s,xbytesview input)
{
    XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return;
    xwsmessageerrorinfo error={0};if(!xrtWsMessagePayload(&s->value,input,&error))xl6WsError("websocket-message",error.Code,error.Offset);
    xl6WsLeave(&guard);
}
void xl6WsMessageStateWritePayloadTextV2(void* raw,xstrview source,int64_t size)
{xbytesview input=xl6WsAsBytes(source);if(xl6BufferSelect(&input,size))payload(raw,input);}
void xl6WsMessageStateWritePayloadSpanV2(void* raw,xbytesview input,int64_t size)
{if(xl6BufferSelect(&input,size))payload(raw,input);}
void xl6WsMessageStateWritePayloadBytesV2(void* raw,xvalue* source,int64_t size)
{xbytesview input;if(xl6BufferBytesInput(source,size,&input))payload(raw,input);}
void xl6WsMessageStateWritePayloadBufferV2(void* raw,void* source,int64_t size)
{xbytesview input;if(xl6BufferInput(source,size,&input))payload(raw,input);}
void xl6WsMessageStateEndFrameV2(void* raw)
{State* s=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return;xwsmessageerrorinfo error={0};if(!xrtWsMessageFrameEnd(&s->value,&error))xl6WsError("websocket-message",error.Code,error.Offset);xl6WsLeave(&guard);}
void xl6WsMessageStateResetV2(void* raw)
{State* s=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return;xrtWsMessageReset(&s->value);xl6WsLeave(&guard);}
int64_t xl6WsMessageStateActiveSizeV2(void* raw)
{State* s=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return 0;int64_t result=(int64_t)s->value.Size;xl6WsLeave(&guard);return result;}
bool xl6WsMessageStateFailedV2(void* raw)
{State* s=raw;XL6_WsGuard guard={0};if(!xl6WsEnter(&s->gate,&guard))return false;bool result=s->value.Failed;xl6WsLeave(&guard);return result;}
void xl6WsMessageInfoDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6WsMessageInfoFlagsV2(void* raw){return ((xwsmessageinfo*)raw)->Flags;}
int64_t xl6WsMessageInfoRsvV2(void* raw){return ((xwsmessageinfo*)raw)->Rsv;}
int64_t xl6WsMessageInfoOpcodeV2(void* raw){return ((xwsmessageinfo*)raw)->Opcode;}
int64_t xl6WsMessageInfoFrameOpcodeV2(void* raw){return ((xwsmessageinfo*)raw)->FrameOpcode;}
uint64_t xl6WsMessageInfoPayloadSizeV2(void* raw){return ((xwsmessageinfo*)raw)->PayloadSize;}
int64_t xl6WsMessageInfoOffsetV2(void* raw){return (int64_t)((xwsmessageinfo*)raw)->Offset;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsMessageConfigHandlePolicyV2Get,xl6WsMessageConfigDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsMessageInfoHandlePolicyV2Get,xl6WsMessageInfoDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsMessageStateHandlePolicyV2Get,xl6WsMessageStateDestroyV2)
