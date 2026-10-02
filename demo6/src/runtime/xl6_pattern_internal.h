#ifndef XL6_PATTERN_INTERNAL_H
#define XL6_PATTERN_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"

/* XRT Value metadata is always NULL. User tags live in managed language fields,
 * never hidden strong references under an opaque native resource. */
typedef struct XL6_PatternOptions {
    xpatternconfig config;
    char* separators;
} XL6_PatternOptions;

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
