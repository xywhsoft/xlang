#include "src/runtime/xl6_text_internal.h"

int64_t xl6TextLengthV1(xstrview text, int64_t size)
{ return xl6TextSelect(&text, size) ? (int64_t)xrtUtf8Count(text) : 0; }
int64_t xl6TextCompareV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize)
{
    if (!xl6TextSelect(&left, leftSize) || !xl6TextSelect(&right, rightSize)) return 0;
    return ignoreCase ? xrtStrCaseCompare(left, right) : xrtStrCompare(left, right);
}
bool xl6TextEqualsV1(xstrview left, xstrview right, bool ignoreCase, int64_t leftSize, int64_t rightSize)
{
    if (!xl6TextSelect(&left, leftSize) || !xl6TextSelect(&right, rightSize)) return false;
    return ignoreCase ? xrtStrCaseEqual(left, right) : xrtStrEqual(left, right);
}
bool xl6TextContainsV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return false;
    return ignoreCase ? xrtStrCaseContains(text, part) : xrtStrContains(text, part);
}
int64_t xl6TextIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return -1;
    size_t found = ignoreCase ? xrtUtf8CaseFind(text, part, 0) : xrtUtf8Find(text, part, 0);
    return found == XRT_NPOS ? -1 : (int64_t)found;
}
int64_t xl6TextLastIndexOfV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return -1;
    size_t found = ignoreCase ? xrtUtf8CaseRFind(text, part) : xrtUtf8RFind(text, part);
    return found == XRT_NPOS ? -1 : (int64_t)found;
}
int64_t xl6TextCountV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return 0;
    return (int64_t)(ignoreCase ? xrtStrCaseCount(text, part) : xrtStrCount(text, part));
}
bool xl6TextStartsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return false;
    return ignoreCase ? xrtStrCaseStarts(text, part) : xrtStrStarts(text, part);
}
bool xl6TextEndsWithV1(xstrview text, xstrview part, bool ignoreCase, int64_t size, int64_t partSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&part, partSize)) return false;
    return ignoreCase ? xrtStrCaseEnds(text, part) : xrtStrEnds(text, part);
}
bool xl6TextHasAnyV1(xstrview text, xstrview chars, int64_t size, int64_t charsSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&chars, charsSize)) return false;
    return xrtUtf8ContainsAny(text, chars);
}
bool xl6TextIsEmptyV1(xstrview text, int64_t size)
{ return xl6TextSelect(&text, size) && xrtStrEmpty(text); }
bool xl6TextIsBlankV1(xstrview text, int64_t size)
{ return xl6TextSelect(&text, size) && xrtStrBlank(text); }
bool xl6TextIsUtf8V1(xstrview text, int64_t size)
{ return xl6TextSelectRaw(&text, size) && xrtUtf8Valid(text, NULL); }
uint32_t xl6TextCharAtV1(xstrview text, int64_t index, int64_t size)
{
    uint32_t scalar = 0;
    if (!xl6TextSelect(&text, size)) return 0;
    int64_t count = (int64_t)xrtUtf8Count(text);
    if (index < 0 && index >= -count) index += count;
    if (index < 0 || index >= count) { xrtSetErrorKind(XERR_RANGE); return 0; }
    if (!xrtUtf8At(text, (size_t)index, &scalar)) return 0;
    return scalar;
}
