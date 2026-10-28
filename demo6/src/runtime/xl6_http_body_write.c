#include "xl6_http_internal.h"
static bool space(xbuffer* out,size_t size)
{if(size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);return false;}return xl6BufferAllocation(xrtBufferReserve(out,out->Size+size));}
int64_t xl6Http1WriteChunkLineV2(uint64_t count,xstrview extensions,void* raw)
{
    xbuffer* out=xl6BufferChecked(raw);size_t size=0;if(!out||!xrtHttp1ChunkLineWrite(count,extensions,NULL,0,&size)||!space(out,size))return 0;
    if(!xrtHttp1ChunkLineWrite(count,extensions,out->Data+out->Size,size,&size))return 0;
    out->Size+=size;return (int64_t)size;
}
xvalue* xl6Http1ChunkLineV2(uint64_t count,xstrview extensions)
{xbuffer out={0};xl6Http1WriteChunkLineV2(count,extensions,&out);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
int64_t xl6Http1WriteChunkSpanV2(xbytesview input,void* raw,int64_t size)
{
    xbuffer* out;size_t count=0;
    if(!xl6BufferSelect(&input,size)||!xl6HttpOutput(raw,input,&out)||!xrtHttp1ChunkWrite(input,NULL,0,&count)||!space(out,count))return 0;
    if(!xrtHttp1ChunkWrite(input,out->Data+out->Size,count,&count))return 0;
    out->Size+=count;return (int64_t)count;
}
int64_t xl6Http1WriteChunkBytesV2(xvalue* input,void* out,int64_t size)
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?xl6Http1WriteChunkSpanV2(view,out,-1):0;}
int64_t xl6Http1WriteChunkBufferV2(void* input,void* out,int64_t size)
{xbytesview view;return xl6BufferInput(input,size,&view)?xl6Http1WriteChunkSpanV2(view,out,-1):0;}
xvalue* xl6Http1ChunkSpanV2(xbytesview input,int64_t size)
{xbuffer out={0};xl6Http1WriteChunkSpanV2(input,&out,size);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
xvalue* xl6Http1ChunkBytesV2(xvalue* input,int64_t size)
{xbytesview view;return xl6BufferBytesInput(input,size,&view)?xl6Http1ChunkSpanV2(view,-1):NULL;}
xvalue* xl6Http1ChunkBufferV2(void* input,int64_t size)
{xbytesview view;return xl6BufferInput(input,size,&view)?xl6Http1ChunkSpanV2(view,-1):NULL;}
int64_t xl6Http1WriteChunkEndV2(void* trailers,void* raw)
{
    xbuffer* out=xl6BufferChecked(raw);XL6_HttpGuard guard={0};const xhttpfield* fields;size_t n,size=0;
    if(!out||!xl6HttpHeadersBorrow(trailers,&guard,&fields,&n))return 0;
    bool ok=xrtHttp1ChunkEndWrite(fields,n,NULL,0,&size)&&space(out,size);
    if(ok)ok=xrtHttp1ChunkEndWrite(fields,n,out->Data+out->Size,size,&size);
    if(ok)out->Size+=size;
    xl6HttpLeave(&guard);return ok?(int64_t)size:0;
}
xvalue* xl6Http1ChunkEndV2(void* trailers)
{xbuffer out={0};xl6Http1WriteChunkEndV2(trailers,&out);xvalue* value=xrtGetError()?NULL:xl6HttpTakeBuffer(&out);xrtBufferUnit(&out);return value;}
