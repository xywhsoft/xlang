#ifndef XL6_WS_STREAM_SEND_IMPL_H
#define XL6_WS_STREAM_SEND_IMPL_H
#include "xl6_ws_stream_internal.h"
#define WS_SEND(Name,Native) \
static int64_t Name##View(void* raw,xbytesview bytes,int64_t size) { \
 if(!xl6BufferSelect(&bytes,size)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;} \
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return XNET_RESULT_ERROR; \
 xnetresult result=Native;xl6ExecutionResourceLeaveV1(&scope);if(result==XNET_RESULT_ERROR)xl6WsStreamFailV2();return result;} \
int64_t xl6WsStream##Name##TextV2(void* r,xstrview b,int64_t n){return Name##View(r,xl6WsAsBytes(b),n);} \
int64_t xl6WsStream##Name##SpanV2(void* r,xbytesview b,int64_t n){return Name##View(r,b,n);} \
int64_t xl6WsStream##Name##BytesV2(void* r,xvalue* b,int64_t n){xbytesview v;if(!xl6BufferBytesInput(b,n,&v)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;}return Name##View(r,v,-1);} \
int64_t xl6WsStream##Name##BufferV2(void* r,void* b,int64_t n){xbytesview v;if(!xl6BufferInput(b,n,&v)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;}return Name##View(r,v,-1);}
#endif
