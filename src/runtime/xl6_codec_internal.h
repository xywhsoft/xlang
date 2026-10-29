#ifndef XL6_CODEC_INTERNAL_H
#define XL6_CODEC_INTERNAL_H
#include "xl6_buffer_internal.h"
xstrview xl6CodecHexViewV1(xbytesview,int64_t);
xstrview xl6CodecBase64ViewV1(xbytesview,int64_t,xstrview,int64_t);
xstrview xl6CodecPercentViewV1(xbytesview,xstrview,bool,bool,int64_t);

static inline bool xl6CodecFlags(int64_t flags)
{
    if (flags<0 || (uint64_t)flags>UINT32_MAX) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
static inline bool xl6CodecBase64Config(int64_t flags,xstrview alphabet,int64_t size,
    xbase64config* config,char storage[65])
{
    if (!xl6CodecFlags(flags) || !xl6TextSelectRaw(&alphabet,size)) return false;
    if (alphabet.Size && alphabet.Size!=64) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    config->Flags=(uint32_t)flags;config->Alphabet=NULL;
    if (alphabet.Size) {memcpy(storage,alphabet.Data,64);storage[64]=0;config->Alphabet=storage;}
    return true;
}
/* Take owns the allocation on every path, including value allocation failure. */
static inline xvalue* xl6CodecBytesTake(bytes data,size_t size)
{
    if (!data) {if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);return NULL;}
    if (size>INT64_MAX) {xrtFree(data);xrtSetErrorKind(XERR_RANGE);return NULL;}
    xvalue* result=xrtValueBytesTake(&data,size);
    xerror* error=xrtTakeError();xrtFree(data);xrtClearError();xrtSetErrorTake(error);
    return result;
}
#endif
