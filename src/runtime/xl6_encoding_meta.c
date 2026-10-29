#include "src/runtime/xl6_text_internal.h"
#include <string.h>

/* Query/name linkage has no Value/Buffer/charset-detection dependency. */
static bool xl6EncodingMetaValid(int64_t encoding)
{
    if (encoding < XENCODING_UNKNOWN || encoding > XENCODING_UTF32_BE) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
int64_t xl6EncodingUnitSizeV1(int64_t encoding)
{ return xl6EncodingMetaValid(encoding) ? (int64_t)xrtEncodingUnitSize((xencoding)encoding) : 0; }
xstrview xl6EncodingNameV1(int64_t encoding)
{
    static const xstrview names[] = {{"unknown",7},{"utf-8",5},{"utf-16le",8},
        {"utf-16be",8},{"utf-32le",8},{"utf-32be",8}};
    char* data;
    xstrview name;
    if (!xl6EncodingMetaValid(encoding)) return xl6TextFailure;
    name = names[encoding];
    data = xrtMalloc(name.Size + 1);
    if (data) memcpy(data, name.Data, name.Size + 1);
    return xl6TextOwn(data, name.Size);
}
