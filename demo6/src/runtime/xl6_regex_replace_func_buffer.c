#include "src/runtime/xl6_regex_internal.h"
#include "src/runtime/xl6_closure.h"

int64_t xl6RegexReplacerRunBufferV1(void* handle,void* target,XL6_Closure* callback)
{
    if (!target) {xrtSetErrorKind(XERR_ARGUMENT);return 0;}
    xstrbuf output;xrtStrBufInit(&output);size_t count=0;
    bool ok=xl6RegexReplacerRunOutputV1(handle,callback,&output,&count);
    if (ok && count>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);ok=false;}
    if (ok) ok=xrtBufferAppend(target,(xbytesview){(const uint8*)output.Data,output.Size});
    xrtStrBufFree(&output);return ok?(int64_t)count:0;
}
