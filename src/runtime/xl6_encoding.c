#include "src/runtime/xl6_unicode_internal.h"
#include <string.h>

/* Native Unicode codecs remain the sole transcode implementation. This
 * boundary validates language options, borrows exact bytes, handles explicit
 * BOM policy and transfers one allocation into the owned language result. */
static bool xl6EncodingOptions(int64_t encoding, int64_t policy)
{
    if (encoding < XENCODING_UTF8 || encoding > XENCODING_UTF32_BE ||
        policy < XUTF_STRICT || policy > XUTF_REPLACE) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
static bool xl6EncodingSource(xstrview* input, int64_t encoding, int64_t bom)
{
    unsigned char expected[4];
    size_t width;
    bool matches;
    if (bom < 0 || bom > 3) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (bom == 0) return true; /* Literal data, including an initial U+FEFF. */
    width = xrtEncodingWriteBom((xencoding)encoding, expected, sizeof(expected));
    matches = input->Size >= width && !memcmp(input->Data, expected, width);
    /* Prefer the explicitly selected encoding: FF FE 00 00 can mean a UTF-16
     * BOM followed by U+0000, not necessarily a UTF-32 BOM. Generic guessing
     * must never override that unambiguous caller contract. */
    if (bom == 3) {
        if (matches || xrtEncodingBom((xbytesview){(const unsigned char*)input->Data, input->Size}, NULL) != XENCODING_UNKNOWN) {
            xrtSetErrorKind(XERR_ARGUMENT); return false;
        }
    } else if (matches) { input->Data += width; input->Size -= width; }
    else if (bom == 2 || xrtEncodingBom((xbytesview){(const unsigned char*)input->Data, input->Size}, NULL) != XENCODING_UNKNOWN) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
static xvalue* xl6EncodingTake(unsigned char* data, size_t size)
{
    xvalue* result;
    if (!data) return NULL;
    if (size > INT64_MAX) { xrtFree(data); xrtSetErrorKind(XERR_RANGE); return NULL; }
    result = xrtValueBytesTake(&data, size);
    if (!result) xrtFree(data);
    return result;
}
xvalue* xl6EncodingEncodeV1(xstrview text, int64_t encoding, bool bom, int64_t policy, int64_t size)
{
    size_t output = 0;
    unsigned char* data;
    if (!xl6EncodingOptions(encoding, policy) || !xl6TextSelectRaw(&text, size)) return NULL;
    data = xrtTranscode((xbytesview){(const unsigned char*)text.Data, text.Size},
        XENCODING_UTF8, (xencoding)encoding, (xutfpolicy)policy, bom, &output);
    return xl6EncodingTake(data, output);
}
xstrview xl6EncodingDecodeV1(const xvalue* value, int64_t encoding, int64_t policy, int64_t bom, int64_t size)
{
    xstrview input;
    size_t output = 0;
    unsigned char* data;
    if (!xl6EncodingOptions(encoding, policy) || !xl6UnicodeBytes(value, size, &input) ||
        !xl6EncodingSource(&input, encoding, bom)) return xl6TextFailure;
    data = xrtTranscode((xbytesview){(const unsigned char*)input.Data, input.Size},
        (xencoding)encoding, XENCODING_UTF8, (xutfpolicy)policy, false, &output);
    return xl6TextOwn((char*)data, output);
}
xstrview xl6EncodingRepairTextV1(xstrview text, int64_t size)
{
    size_t output = 0;
    unsigned char* data;
    if (!xl6TextSelectRaw(&text, size)) return xl6TextFailure;
    data = xrtTranscode((xbytesview){(const unsigned char*)text.Data, text.Size},
        XENCODING_UTF8, XENCODING_UTF8, XUTF_REPLACE, false, &output);
    return xl6TextOwn((char*)data, output);
}
xvalue* xl6EncodingTranscodeV1(const xvalue* value, int64_t source, int64_t target, int64_t policy, int64_t bom, int64_t size)
{
    xstrview input;
    size_t output = 0;
    unsigned char* data;
    if (!xl6EncodingOptions(source, policy) || !xl6EncodingOptions(target, policy)) return NULL;
    if (bom < 0 || bom > 7) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xl6UnicodeBytes(value, size, &input) || !xl6EncodingSource(&input, source, bom & 3)) return NULL;
    data = xrtTranscode((xbytesview){(const unsigned char*)input.Data, input.Size},
        (xencoding)source, (xencoding)target, (xutfpolicy)policy, (bom & 4) != 0, &output);
    return xl6EncodingTake(data, output);
}
int64_t xl6EncodingBomV1(const xvalue* value, int64_t size)
{
    xstrview input;
    return xl6UnicodeBytes(value, size, &input)
        ? (int64_t)xrtEncodingBom((xbytesview){(const unsigned char*)input.Data, input.Size}, NULL) : 0;
}
int64_t xl6EncodingBomSizeV1(const xvalue* value, int64_t size)
{
    xstrview input;
    size_t width = 0;
    if (xl6UnicodeBytes(value, size, &input))
        (void)xrtEncodingBom((xbytesview){(const unsigned char*)input.Data, input.Size}, &width);
    return (int64_t)width;
}
xvalue* xl6EncodingBomBytesV1(int64_t encoding)
{
    unsigned char data[4];
    size_t width;
    if (!xl6EncodingOptions(encoding, XUTF_STRICT)) return NULL;
    width = xrtEncodingWriteBom((xencoding)encoding, data, sizeof(data));
    return xrtValueBytes((xbytesview){data, width});
}

/* Synchronous input spans; no preliminary managed bytes allocation. */
xstrview xl6EncodingDecodeSpanV1(xbytesview value, int64_t encoding, int64_t policy, int64_t bom, int64_t size)
{
    xstrview input;
    size_t output = 0;
    unsigned char* data;
    if (!xl6EncodingOptions(encoding, policy) || !xl6UnicodeSpan(value, size, &input) ||
        !xl6EncodingSource(&input, encoding, bom)) return xl6TextFailure;
    data = xrtTranscode((xbytesview){(const unsigned char*)input.Data, input.Size},
        (xencoding)encoding, XENCODING_UTF8, (xutfpolicy)policy, false, &output);
    return xl6TextOwn((char*)data, output);
}

xvalue* xl6EncodingTranscodeSpanV1(xbytesview value, int64_t source, int64_t target, int64_t policy, int64_t bom, int64_t size)
{
    xstrview input;
    size_t output = 0;
    unsigned char* data;
    if (!xl6EncodingOptions(source, policy) || !xl6EncodingOptions(target, policy)) return NULL;
    if (bom < 0 || bom > 7) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xl6UnicodeSpan(value, size, &input) || !xl6EncodingSource(&input, source, bom & 3)) return NULL;
    data = xrtTranscode((xbytesview){(const unsigned char*)input.Data, input.Size},
        (xencoding)source, (xencoding)target, (xutfpolicy)policy, (bom & 4) != 0, &output);
    return xl6EncodingTake(data, output);
}

int64_t xl6EncodingBomSpanV1(xbytesview value, int64_t size)
{
    xstrview input;
    return xl6UnicodeSpan(value, size, &input)
        ? (int64_t)xrtEncodingBom((xbytesview){(const unsigned char*)input.Data, input.Size}, NULL) : 0;
}

int64_t xl6EncodingBomSizeSpanV1(xbytesview value, int64_t size)
{
    xstrview input;
    size_t width = 0;
    if (xl6UnicodeSpan(value, size, &input))
        (void)xrtEncodingBom((xbytesview){(const unsigned char*)input.Data, input.Size}, &width);
    return (int64_t)width;
}
