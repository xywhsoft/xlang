#ifndef XL6_PATTERN_INTERNAL_H
#define XL6_PATTERN_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"

/* XRT Value metadata is always NULL. User tags live in managed language fields,
 * never hidden strong references under an opaque native resource. */
typedef struct XL6_PatternOptions {
    xpatternconfig config;
    char* separators;
} XL6_PatternOptions;

/* A bijection from the opaque unsigned ID into the existing signed int-map
 * key domain. Do not decode generation/slot bits from an XRT implementation. */
static inline int64_t xl6PatternTagKeyFromId(xpatternid id)
{return id<=INT64_MAX?(int64_t)id:-1-(int64_t)(UINT64_MAX-id);}

static inline bool xl6PatternIndex(const xpattern* handle,int64_t index)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if (index<0 || (uint64_t)index>=xrtPatternCount(handle)) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
static inline bool xl6PatternInput(const void* handle,xstrview* text,int64_t size)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return xl6TextSelect(text,size);
}
#endif
