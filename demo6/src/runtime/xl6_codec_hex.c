#include "xl6_codec_internal.h"

xstrview xl6CodecHexViewV1(xbytesview input,int64_t flags)
{
    size_t size=0;
    if (!xl6CodecFlags(flags) || !xrtHexEncode(input.Data,input.Size,NULL,0,&size,(uint32_t)flags)) return xl6TextFailure;
    if (size>=SIZE_MAX || size>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    char* output=xrtMalloc(size+1);if (!output) return xl6TextOwn(NULL,0);
    if (!xrtHexEncode(input.Data,input.Size,output,size+1,&size,(uint32_t)flags)) {
        xerror* error=xrtTakeError();xrtFree(output);xrtClearError();xrtSetErrorTake(error);return xl6TextFailure;
    }
    return xl6TextOwn(output,size);
}
xstrview xl6CodecHexTextV1(xstrview text,int64_t size,int64_t flags)
{
    return xl6TextSelectRaw(&text,size)?xl6CodecHexViewV1((xbytesview){(const uint8*)text.Data,text.Size},flags):xl6TextFailure;
}
