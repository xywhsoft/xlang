#ifndef XL6_TEXT_INTERNAL_H
#define XL6_TEXT_INTERNAL_H
#include "src/runtime/xl6_text_value.h"
#include <limits.h>

/* Byte operations may select a prefix within a UTF-8 scalar. String
 * operations additionally validate the selected UTF-8 view. */
static inline bool xl6TextSelectRaw(xstrview* text, int64_t size)
{
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!text->Data && text->Size) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (size >= 0 && (uint64_t)size < text->Size) text->Size = (size_t)size;
    if (text->Size > INT64_MAX || text->Size == SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!text->Data) text->Data = "";
    return true;
}
static inline bool xl6TextSelect(xstrview* text, int64_t size)
{
    if (!xl6TextSelectRaw(text, size)) return false;
    if (xrtUtf8Count(*text) != XRT_NPOS) return true;
    xstrview ignored;
    return xrtUtf8Range(*text, 0, 0, &ignored);
}
static const xstrview xl6TextFailure = {NULL, 0};
static inline xstrview xl6TextOwn(char* data, size_t size)
{
    if (!data) {
        if (xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorKind(XERR_MEMORY);
        return xl6TextFailure;
    }
    if (size > INT64_MAX) { xrtFree(data); xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    return (xstrview){data, size};
}
static inline xstrview xl6TextCopy(xstrview text)
{ return xl6TextOwn(xrtStrDupView(text), text.Size); }
#endif
