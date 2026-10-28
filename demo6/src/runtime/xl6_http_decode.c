#include "xl6_http_internal.h"
typedef struct XL6_HttpContentDecoder {xatomic32 gate;xhttpdecode* native;bool failed;} XL6_HttpContentDecoder;
static bool config(xhttpdecodeconfig* config,int64_t limit,int64_t header,int64_t codings,bool raw)
{
    if(limit< -1||header<0||(uint64_t)header>UINT32_MAX||codings<1||codings>XHTTP_CONTENT_CODINGS_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    xrtHttpDecodeConfigInitSafe(config);config->OutputLimit=limit<0?UINT64_MAX:(uint64)limit;
    config->GzipHeaderLimit=(uint32)header;config->MaxCodings=(uint32)codings;config->Flags=raw?XHTTP_DECODE_ALLOW_RAW:0;return true;
}
void* xl6HttpContentDecoderCreateV2(void* headers,int64_t limit,int64_t header,int64_t codings,bool raw)
{
    xhttpdecodeconfig options;if(!config(&options,limit,header,codings,raw))return NULL;
    XL6_HttpGuard g={0};const xhttpfield* fields;size_t count;if(!xl6HttpHeadersBorrow(headers,&g,&fields,&count))return NULL;
    xhttpdecode* native=xrtHttpDecodeCreate(fields,count,&options);xl6HttpLeave(&g);if(!native)return NULL;
    XL6_HttpContentDecoder* self=xrtCalloc(1,sizeof(*self));if(!self){xrtHttpDecodeDestroy(native);return NULL;}self->native=native;return self;
}
void xl6HttpContentDecoderDestroyV2(void* raw)
{XL6_HttpContentDecoder* self=raw;if(self){xrtHttpDecodeDestroy(self->native);xrtFree(self);}}
void xl6HttpContentDecoderResetV2(void* raw,void* headers,int64_t limit,int64_t header,int64_t codings,bool passthrough)
{
    XL6_HttpContentDecoder* self=raw;xhttpdecodeconfig options;XL6_HttpGuard g={0},h={0};const xhttpfield* fields;size_t count;
    if(!config(&options,limit,header,codings,passthrough)||!xl6HttpEnter(&self->gate,&g))return;
    if(xl6HttpHeadersBorrow(headers,&h,&fields,&count)){
        bool ok=xrtHttpDecodeReset(self->native,fields,count,&options);self->failed=!ok;xl6HttpLeave(&h);
    }xl6HttpLeave(&g);
}
static bool output(xbytesview data,ptr target){return xl6HttpAppend(target,data);}
void xl6HttpContentDecoderWriteSpanV2(void* raw,xbytesview input,void* target,bool final,int64_t size)
{
    XL6_HttpContentDecoder* self=raw;xbuffer* out;XL6_HttpGuard g={0};
    if(!xl6BufferSelect(&input,size)||!xl6HttpOutput(target,input,&out)||!xl6HttpEnter(&self->gate,&g))return;
    if(self->failed)xrtSetErrorKind(XERR_STATE);
    else if(!xrtHttpDecodeWrite(self->native,input,final,output,out))self->failed=true;
    xl6HttpLeave(&g);
}
void xl6HttpContentDecoderWriteBytesV2(void* self,xvalue* input,void* out,bool final,int64_t size)
{xbytesview view;if(xl6BufferBytesInput(input,size,&view))xl6HttpContentDecoderWriteSpanV2(self,view,out,final,-1);}
void xl6HttpContentDecoderWriteBufferV2(void* self,void* input,void* out,bool final,int64_t size)
{xbytesview view;if(xl6BufferInput(input,size,&view))xl6HttpContentDecoderWriteSpanV2(self,view,out,final,-1);}
void xl6HttpContentDecoderFinishV2(void* self,void* out)
{xl6HttpContentDecoderWriteSpanV2(self,(xbytesview){NULL,0},out,true,-1);}
#define QUERY(Name,Type,Value,Failure) Type xl6HttpContentDecoder##Name##V2(void* raw) \
{XL6_HttpContentDecoder* self=raw;XL6_HttpGuard g={0};if(!xl6HttpEnter(&self->gate,&g))return Failure;Type value=(Value);xl6HttpLeave(&g);return value;}
QUERY(Done,bool,xrtHttpDecodeDone(self->native),false)
QUERY(Failed,bool,self->failed,false)
QUERY(Mode,int64_t,(int64_t)xrtHttpDecodeMode(self->native),0)
QUERY(InputSize,uint64_t,xrtHttpDecodeInputSize(self->native),0)
QUERY(OutputSize,uint64_t,xrtHttpDecodeOutputSize(self->native),0)
#undef QUERY
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpContentDecoderHandlePolicyV2Get,xl6HttpContentDecoderDestroyV2)
