#include "xl6_ws_stream_internal.h"
#include "xl6_net_bytes_internal.h"
int64_t xl6WsStreamSendRefV2(void* raw,int64_t opcode,void* input,int64_t size)
{xbytesview view;if(!xl6NetBytesRangeV1(input,size,0,&view)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;}
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return XNET_RESULT_ERROR;
 xnetresult result=xrtWsStreamSendBytes(s,(xwsopcode)opcode,input,0,view.Size);
 xl6ExecutionResourceLeaveV1(&scope);if(result==XNET_RESULT_ERROR)xl6WsStreamFailV2();return result;}
