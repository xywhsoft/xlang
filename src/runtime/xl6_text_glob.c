#include "src/runtime/xl6_text_internal.h"

bool xl6TextGlobV1(xstrview text, xstrview pattern, int64_t flags, int64_t size, int64_t patternSize)
{
    if (flags < 0 || (uint64_t)flags > XSTR_GLOB_CASE_ASCII) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&pattern, patternSize)) return false;
    return xrtStrGlob(text, pattern, (uint32)flags);
}
bool xl6TextLikeV1(xstrview text, xstrview pattern, bool ignoreCase, int64_t size, int64_t patternSize)
{ return xl6TextGlobV1(text, pattern, ignoreCase ? XSTR_GLOB_CASE_ASCII : 0, size, patternSize); }
