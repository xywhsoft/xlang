#include "xl6_ws_stream_internal.h"
int64_t xl6WsStreamSendTakeV2(void* raw,int64_t opcode,void* input,int64_t size)
{
 xbuffer* b=xl6BufferChecked(input);xbytesview view;if(!b||!xl6BufferInput(input,size,&view)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;}
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return XNET_RESULT_ERROR;
 /* A prefix cannot transfer an allocation containing an unconsumed suffix. */
 if(view.Size!=b->Size){xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_ARGUMENT);xl6WsStreamFailV2();return XNET_RESULT_ERROR;}
 /* Mutable Buffer cannot be retained as immutable. Copy before consuming its
  * allocator-owned storage; the typed NetBytes path supplies retained sends. */
 xnetresult result=xrtWsStreamSend(s,(xwsopcode)opcode,view);
 if(result==XNET_RESULT_OK)xrtBufferUnit(b);
 xl6ExecutionResourceLeaveV1(&scope);if(result==XNET_RESULT_ERROR)xl6WsStreamFailV2();return result;
}
