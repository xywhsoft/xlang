#ifndef XL6_COMPRESS_INTERNAL_H
#define XL6_COMPRESS_INTERNAL_H
#include "xl6_codec_internal.h"
#include "xl6_text_thread.h"
static inline bool xl6CompressBufferInput(void* raw,int64_t size,xbytesview* input)
{
    xbuffer* buffer=xl6BufferChecked(raw);if (!buffer) return false;
    *input=(xbytesview){buffer->Data,buffer->Size};return xl6BufferSelect(input,size);
}

static inline bool xl6CompressLimit(int64_t limit,uint64* result)
{
    if (limit < -1) {xrtSetErrorKind(XERR_RANGE);return false;}
    *result=limit==-1?UINT64_MAX:(uint64)limit;return true;
}
static inline bool xl6DeflateConfig(xdeflateconfig* config,int64_t format,int64_t level,
    int64_t strategy,int64_t limit,int64_t window)
{
    if (format<0 || format>2 || level<0 || level>10 || strategy<0 || strategy>4 || window<8 || window>15) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    xrtDeflateConfigInit(config);config->Format=(xdeflateformat)format;
    config->Level=(int32)level;config->Strategy=(xdeflatestrategy)strategy;config->WindowBits=(uint8)window;
    return xl6CompressLimit(limit,&config->OutputLimit) && xrtDeflateConfigValid(config);
}
static inline bool xl6InflateConfig(xinflateconfig* config,int64_t format,int64_t limit,int64_t header,int64_t window)
{
    if (format<0 || format>3 || header<0 || (uint64_t)header>UINT32_MAX || window<8 || window>15) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    xrtInflateConfigInit(config);
    config->Format=format==2?XINFLATE_GZIP:format==3?XINFLATE_DEFLATE:(xinflateformat)format;
    config->WindowBits=(uint8)window;config->GzipHeaderLimit=(uint32)header;
    return xl6CompressLimit(limit,&config->OutputLimit) && xrtInflateConfigValid(config);
}
static inline bool xl6CompressOutput(xbytesview data,ptr target)
{
    xbuffer* buffer=target;
    if (data.Size>(size_t)INT64_MAX-buffer->Size) {xrtSetErrorKind(XERR_RANGE);return false;}
    bool ok=xrtBufferAppend(buffer,data);
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return ok;
}
/* Native consumers must not invalidate a borrowed input by appending into the
 * same allocation. Check address arithmetic before touching the stream. */
static inline bool xl6CompressOutputTarget(void* raw,xbytesview input,xbuffer** output)
{
    *output=xl6BufferChecked(raw);if (!*output) return false;
    uintptr_t a=(uintptr_t)input.Data,b=(uintptr_t)(*output)->Data;
    size_t capacity=(*output)->Capacity;
    if ((input.Size && (!a || input.Size>UINTPTR_MAX-a)) || (capacity && (!b || capacity>UINTPTR_MAX-b)) ||
        (input.Size && capacity && a<b+capacity && b<a+input.Size)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return true;
}
#endif
