#include "src/runtime/xl6_unicode_internal.h"

static XL6_ProductI64x3V1 xl6EncodingGuess(xstrview view)
{
    xencodingguess guess = xrtEncodingGuess((xbytesview){(const unsigned char*)view.Data,view.Size});
    return (XL6_ProductI64x3V1){(int64_t)guess.Encoding,(int64_t)guess.BomSize,(int64_t)guess.Confidence};
}
XL6_ProductI64x3V1 xl6EncodingGuessTextV1(xstrview text, int64_t size)
{ return xl6TextSelectRaw(&text,size) ? xl6EncodingGuess(text) : (XL6_ProductI64x3V1){0,0,0}; }
XL6_ProductI64x3V1 xl6EncodingGuessBytesV1(const xvalue* data, int64_t size)
{
    xstrview view;
    return xl6UnicodeBytes(data,size,&view) ? xl6EncodingGuess(view) : (XL6_ProductI64x3V1){0,0,0};
}
