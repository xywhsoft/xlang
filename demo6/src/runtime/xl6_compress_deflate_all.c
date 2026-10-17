#include "xl6_compress_internal.h"
static xvalue* encode(xbytesview input,int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{
    xdeflateconfig config;size_t size=0;
    if (!xl6DeflateConfig(&config,format,level,strategy,limit,window)) return NULL;
    bytes result=xrtDeflateAll(input,&config,&size);return xl6CodecBytesTake(result,size);
}
xvalue* xl6DeflateBytesV1(const xvalue* value,int64_t size,int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{xbytesview input;return xl6BufferBytesInput(value,size,&input)?encode(input,format,level,strategy,limit,window):NULL;}
xvalue* xl6DeflateTextV1(xstrview text,int64_t size,int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{return xl6TextSelectRaw(&text,size)?encode((xbytesview){(const uint8*)text.Data,text.Size},format,level,strategy,limit,window):NULL;}
xvalue* xl6DeflateBufferV1(void* value,int64_t size,int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{xbytesview input;return xl6CompressBufferInput(value,size,&input)?encode(input,format,level,strategy,limit,window):NULL;}
