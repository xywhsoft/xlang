#include "xl6_codec_internal.h"

xstrview xl6CodecPercentViewV1(xbytesview input,xstrview safe,bool form,bool unreserved,int64_t safeSize)
{
    xpercentmap map;size_t size=0;
    if (!xl6TextSelectRaw(&safe,safeSize) || !xrtPercentMapInit(&map,safe,unreserved)) return xl6TextFailure;
    /* Form encoding always protects a literal '+', even if in ExtraSafe. */
    if (form) map.Bits['+'>>6]&=~(UINT64_C(1)<<('+'&63));
    if (!xrtPercentMeasure(input.Data,input.Size,&map,form,&size)) return xl6TextFailure;
    if (size>=SIZE_MAX || size>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    char* output=xrtMalloc(size+1);if (!output) return xl6TextOwn(NULL,0);
    xrtPercentEncodeMeasured(input.Data,input.Size,&map,form,output,size,true);
    return xl6TextOwn(output,size);
}
xstrview xl6CodecPercentTextV1(xstrview text,int64_t size,xstrview safe,bool form,bool unreserved,int64_t safeSize)
{
    return xl6TextSelectRaw(&text,size)?xl6CodecPercentViewV1((xbytesview){(const uint8*)text.Data,text.Size},safe,form,unreserved,safeSize):xl6TextFailure;
}
