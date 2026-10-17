#include "xl6_codec_internal.h"

xstrview xl6CodecBase64ViewV1(xbytesview input,int64_t flags,xstrview alphabet,int64_t alphabetSize)
{
    xbase64config config;char storage[65];size_t size=0;
    if (!xl6CodecBase64Config(flags,alphabet,alphabetSize,&config,storage) ||
        !xrtBase64Encode(input.Data,input.Size,NULL,0,&size,&config)) return xl6TextFailure;
    if (size>=SIZE_MAX || size>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    char* output=xrtMalloc(size+1);if (!output) return xl6TextOwn(NULL,0);
    if (!xrtBase64Encode(input.Data,input.Size,output,size+1,&size,&config)) {
        xerror* error=xrtTakeError();xrtFree(output);xrtClearError();xrtSetErrorTake(error);return xl6TextFailure;
    }
    return xl6TextOwn(output,size);
}
xstrview xl6CodecBase64TextV1(xstrview text,int64_t size,int64_t flags,xstrview alphabet,int64_t alphabetSize)
{
    return xl6TextSelectRaw(&text,size)?xl6CodecBase64ViewV1((xbytesview){(const uint8*)text.Data,text.Size},flags,alphabet,alphabetSize):xl6TextFailure;
}
