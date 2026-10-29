#include "xl6_compress_internal.h"
static xvalue* decode(xbytesview input,int64_t format,int64_t limit,int64_t header,int64_t window)
{
    xinflateconfig config;size_t size=0;
    if (!xl6InflateConfig(&config,format,limit,header,window)) return NULL;
    bytes result=xrtInflateAll(input,&config,&size);return xl6CodecBytesTake(result,size);
}
xvalue* xl6InflateBytesV1(const xvalue* value,int64_t size,int64_t format,int64_t limit,int64_t header,int64_t window)
{xbytesview input;return xl6BufferBytesInput(value,size,&input)?decode(input,format,limit,header,window):NULL;}
xvalue* xl6InflateBufferV1(void* value,int64_t size,int64_t format,int64_t limit,int64_t header,int64_t window)
{xbytesview input;return xl6CompressBufferInput(value,size,&input)?decode(input,format,limit,header,window):NULL;}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6InflateBytesSpanV1(xbytesview value,int64_t size,int64_t format,int64_t limit,int64_t header,int64_t window)
{xbytesview input;return xl6BufferSpanInput(value,size,&input)?decode(input,format,limit,header,window):NULL;}
