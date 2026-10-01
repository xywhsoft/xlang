#include "src/runtime/xl6_text_value.h"
#include <limits.h>

static const xstrview xl6TextFailure = {NULL, 0};

static bool xl6TextSelect(xstrview* text, int64_t size)
{
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!text->Data && text->Size) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (size >= 0 && (uint64_t)size < text->Size) text->Size = (size_t)size;
    if (text->Size > INT64_MAX || text->Size == SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!text->Data) text->Data = "";
    /* A byte cap may cut a scalar: reject it, never return invalid string. */
    if (xrtUtf8Count(*text) != XRT_NPOS) return true;
    /* The fast Unicode counter reports a sentinel, not an error object.
     * Reuse XRT's checked range path only on failure for its offset diagnostic. */
    xstrview ignored;
    return xrtUtf8Range(*text, 0, 0, &ignored);
}

static xstrview xl6TextOwn(char* data, size_t size)
{
    if (!data) {
        if (xrtErrorKind(xrtGetError()) == XERR_NONE) xrtSetErrorKind(XERR_MEMORY);
        return xl6TextFailure;
    }
    if (size > INT64_MAX) {
        xrtFree(data); xrtSetErrorKind(XERR_RANGE); return xl6TextFailure;
    }
    return (xstrview){data, size};
}
static xstrview xl6TextCopy(xstrview text)
{ return xl6TextOwn(xrtStrDupView(text), text.Size); }

xstrview xl6TextLowerV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return xl6TextFailure;
    return xl6TextOwn(xrtStrLower(text), text.Size);
}
xstrview xl6TextUpperV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return xl6TextFailure;
    return xl6TextOwn(xrtStrUpper(text), text.Size);
}
static xstrview xl6TextTrim(xstrview text, xstrview chars, int64_t size, int64_t charsSize,
    bool left, bool right)
{
    xstrview range;
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&chars, charsSize)) return xl6TextFailure;
    if (!chars.Size) chars = XRT_STR_LITERAL(" \t\r\n\v\f");
    bool ok = left && right ? xrtUtf8TrimSet(text, chars, &range)
        : left ? xrtUtf8TrimLeftSet(text, chars, &range) : xrtUtf8TrimRightSet(text, chars, &range);
    return ok ? xl6TextCopy(range) : xl6TextFailure;
}
xstrview xl6TextTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize)
{ return xl6TextTrim(text, chars, size, charsSize, true, true); }
xstrview xl6TextLTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize)
{ return xl6TextTrim(text, chars, size, charsSize, true, false); }
xstrview xl6TextRTrimV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize)
{ return xl6TextTrim(text, chars, size, charsSize, false, true); }

xstrview xl6TextFilterV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&chars, charsSize)) return xl6TextFailure;
    char* data = xrtMalloc(text.Size + 1);
    size_t written = 0;
    if (!data) return xl6TextOwn(NULL, 0);
    if (!xrtUtf8FilterTo(text, chars, data, text.Size + 1, &written)) {
        xrtFree(data); return xl6TextFailure;
    }
    return xl6TextOwn(data, written);
}
xstrview xl6TextReplaceV1(xstrview text, xstrview part, xstrview replacement,
    int64_t size, int64_t partSize, int64_t replacementSize)
{
    size_t written = 0;
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize) ||
        !xl6TextSelect(&replacement, replacementSize)) return xl6TextFailure;
    char* data = xrtStrReplaceSized(text, part, replacement, &written);
    return xl6TextOwn(data, written);
}
xstrview xl6TextSliceV1(xstrview text, int64_t start, int64_t count, int64_t size)
{
    xstrview range;
    if (count < -1) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    if (!xl6TextSelect(&text, size) || !xrtUtf8Range(text, start, count, &range)) return xl6TextFailure;
    return xl6TextCopy(range);
}
xstrview xl6TextLeftV1(xstrview text, int64_t count, int64_t size)
{
    if (count < 0) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    return xl6TextSliceV1(text, 0, count, size);
}
xstrview xl6TextRightV1(xstrview text, int64_t count, int64_t size)
{
    if (count < 0) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    /* -0 means index 0, not an empty suffix. Keep zero explicit. */
    return xl6TextSliceV1(text, count == 0 ? 0 : -count, count, size);
}
xstrview xl6TextRepeatV1(xstrview text, int64_t count, int64_t size)
{
    if (count < 0) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    if (!xl6TextSelect(&text, size)) return xl6TextFailure;
    if (text.Size && ((uint64_t)count > SIZE_MAX || (uint64_t)count > INT64_MAX / text.Size)) {
        xrtSetErrorKind(XERR_RANGE); return xl6TextFailure;
    }
    return xl6TextOwn(xrtStrRepeat(text, (size_t)count), text.Size * (size_t)count);
}
xstrview xl6TextReverseV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return xl6TextFailure;
    char* data = xrtMalloc(text.Size + 1);
    if (!data) return xl6TextOwn(NULL, 0);
    if (!xrtUtf8ReverseTo(text, data, text.Size + 1)) { xrtFree(data); return xl6TextFailure; }
    return xl6TextOwn(data, text.Size);
}
xstrview xl6TextRemoveAtV1(xstrview text, int64_t start, int64_t count, int64_t size)
{
    xstrview range;
    if (count < -1) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    if (!xl6TextSelect(&text, size) || !xrtUtf8Range(text, start, count, &range)) return xl6TextFailure;
    return xl6TextOwn(xrtStrRemove(text, (size_t)(range.Data - text.Data), range.Size), text.Size - range.Size);
}
xstrview xl6TextInsertV1(xstrview text, int64_t index, xstrview part, int64_t size, int64_t partSize)
{
    xstrview position;
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize) ||
        !xrtUtf8Range(text, index, 0, &position)) return xl6TextFailure;
    if (part.Size > INT64_MAX - text.Size) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    return xl6TextOwn(xrtStrInsert(text, (size_t)(position.Data - text.Data), part), text.Size + part.Size);
}
