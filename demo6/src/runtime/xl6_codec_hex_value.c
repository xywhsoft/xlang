#include "xl6_codec_internal.h"

xstrview xl6CodecHexBytesV1(const xvalue* value,int64_t size,int64_t flags)
{xbytesview input;return xl6BufferBytesInput(value,size,&input)?xl6CodecHexViewV1(input,flags):xl6TextFailure;}
xvalue* xl6CodecHexDecodeV1(xstrview text,int64_t size,int64_t flags)
{
    if (!xl6TextSelectRaw(&text,size) || !xl6CodecFlags(flags)) return NULL;
    size_t length=0;bytes data=xrtHexDecodeNew(text,&length,(uint32_t)flags);
    return xl6CodecBytesTake(data,length);
}
