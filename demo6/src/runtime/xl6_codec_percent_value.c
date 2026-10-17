#include "xl6_codec_internal.h"

xstrview xl6CodecPercentBytesV1(const xvalue* value,int64_t size,xstrview safe,bool form,bool unreserved,int64_t safeSize)
{xbytesview input;return xl6BufferBytesInput(value,size,&input)?xl6CodecPercentViewV1(input,safe,form,unreserved,safeSize):xl6TextFailure;}
xvalue* xl6CodecPercentDecodeV1(xstrview text,int64_t size,bool form)
{
    size_t length=0;
    if (!xl6TextSelectRaw(&text,size) || !xrtPercentDecodeMeasure(text,form,&length)) return NULL;
    if (length>=SIZE_MAX || length>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    bytes data=xrtMalloc(length+1);if (!data) {xrtSetErrorKind(XERR_MEMORY);return NULL;}
    xrtPercentDecodeMeasured(text,form,data);data[length]=0;
    return xl6CodecBytesTake(data,length);
}
