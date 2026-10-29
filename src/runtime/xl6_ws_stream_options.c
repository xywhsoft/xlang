#include "xl6_ws_internal.h"
void* xl6WsStreamOptionsCreateV2(int64_t message,uint64_t frame,int64_t send,int64_t reserve,int64_t timeout,bool pong)
{
    xwsstreamconfig value;xrtWsStreamConfigInit(&value);
    if(!xl6WsSize(message,&value.MessageLimit)||!xl6WsSize(send,&value.SendLimit)||
       !xl6WsSize(reserve,&value.ControlReserve))return NULL;
    if(timeout<0){xrtSetErrorKind(XERR_RANGE);return NULL;}
    value.FrameLimit=frame;value.CloseTimeout=timeout;value.AutoPong=pong;
    if(!xrtWsStreamConfigValid(&value)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xwsstreamconfig* result=xrtMalloc(sizeof(*result));if(result)*result=value;return result;
}
void xl6WsStreamOptionsDestroyV2(void* self){xrtFree(self);}
void* xl6WsStreamOptionsWithCompressionV2(void* self,void* inflater,void* deflater)
{
    xwsstreamconfig value=*(xwsstreamconfig*)self;
    value.Inflater=*(xwsinflaterconfig*)inflater;value.Deflater=*(xwsdeflaterconfig*)deflater;
    xwsstreamconfig* result=xrtMalloc(sizeof(*result));if(result)*result=value;return result;
}
int64_t xl6WsStreamOptionsMessageLimitV2(void* p){return (int64_t)((xwsstreamconfig*)p)->MessageLimit;}
uint64_t xl6WsStreamOptionsFrameLimitV2(void* p){return ((xwsstreamconfig*)p)->FrameLimit;}
int64_t xl6WsStreamOptionsSendLimitV2(void* p){return (int64_t)((xwsstreamconfig*)p)->SendLimit;}
int64_t xl6WsStreamOptionsControlReserveV2(void* p){return (int64_t)((xwsstreamconfig*)p)->ControlReserve;}
int64_t xl6WsStreamOptionsCloseTimeoutMillisV2(void* p){return (int64_t)((xwsstreamconfig*)p)->CloseTimeout;}
bool xl6WsStreamOptionsAutoPongV2(void* p){return ((xwsstreamconfig*)p)->AutoPong;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsStreamOptionsHandlePolicyV2Get,xl6WsStreamOptionsDestroyV2)
