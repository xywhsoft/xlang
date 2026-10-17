#include "src/runtime/xl6_codec_internal.h"

void* xl6BufferFromBase64V1(xstrview text, int64_t flags, xstrview alphabet,
    int64_t size, int64_t alphabetSize)
{
    xbase64config config; char storage[65];
    if (!xl6TextSelectRaw(&text, size) || !xl6CodecBase64Config(flags, alphabet, alphabetSize, &config, storage)) return NULL;
    xbuffer* result = xrtBufferFromBase64(text, &config);
    xl6BufferAllocation(result != NULL);
    return result;
}
xstrview xl6BufferBase64V1(void* raw, int64_t flags, xstrview alphabet,
    int64_t size, int64_t alphabetSize)
{
    xbytesview input;
    if (!xl6BufferInput(raw,size,&input)) return xl6TextFailure;
    return xl6CodecBase64ViewV1(input,flags,alphabet,alphabetSize);
}
