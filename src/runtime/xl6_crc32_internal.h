#ifndef XL6_CRC32_INTERNAL_H
#define XL6_CRC32_INTERNAL_H
#include "xl6_text_internal.h"
static inline bool xl6Crc32Previous(uint64_t previous)
{
    if (previous > UINT32_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
#endif
