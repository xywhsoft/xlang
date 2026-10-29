#include "xl6_ws_stream_send_impl.h"
WS_SEND(SendTextCompressed,xrtWsStreamTextCompressed(s,xl6WsAsText(bytes)))
WS_SEND(SendBinaryCompressed,xrtWsStreamBinaryCompressed(s,bytes))
bool xl6WsStreamDeflateEnabledV2(void* raw)
{
    XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;
    if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return false;
    xwsdeflate value;bool result=xrtWsStreamDeflate(s,&value);
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
void* xl6WsStreamDeflateResponseV2(void* raw)
{
    XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;
    if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return NULL;
    xwsdeflate value;XL6_WsDeflate* result=NULL;
    if(xrtWsStreamDeflate(s,&value)){result=xrtMalloc(sizeof(*result));if(result)result->value=value;}
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
