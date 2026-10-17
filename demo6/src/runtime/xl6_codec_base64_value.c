#include "xl6_codec_internal.h"

xstrview xl6CodecBase64BytesV1(const xvalue* value,int64_t size,int64_t flags,xstrview alphabet,int64_t alphabetSize)
{xbytesview input;return xl6BufferBytesInput(value,size,&input)?xl6CodecBase64ViewV1(input,flags,alphabet,alphabetSize):xl6TextFailure;}
xvalue* xl6CodecBase64DecodeV1(xstrview text,int64_t size,int64_t flags,xstrview alphabet,int64_t alphabetSize)
{
    xbase64config config;char storage[65];size_t length=0;
    if (!xl6TextSelectRaw(&text,size) || !xl6CodecBase64Config(flags,alphabet,alphabetSize,&config,storage)) return NULL;
    bytes data=xrtBase64DecodeNew(text.Data,text.Size,&length,&config);
    return xl6CodecBytesTake(data,length);
}
