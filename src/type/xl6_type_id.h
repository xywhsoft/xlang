#ifndef XL6_TYPE_ID_H
#define XL6_TYPE_ID_H

#include "src/common/xl6_common.h"

typedef uint32 XL6_TypeId;
typedef uint32 XL6_SymbolId;
typedef uint64 XL6_TypeAbiId;

#define XL6_TYPE_ID_INVALID UINT32_C(0)
#define XL6_SYMBOL_ID_INVALID UINT32_C(0)
#define XL6_TYPE_ABI_ID_INVALID UINT64_C(0)

/*
 * ABI 身份使用标准 FNV-1a 64 位参数。
 * 输入必须是已经包含模块归属和泛型形态的规范名称。
 */
static inline uint64 xl6TypeAbiIdBytes(uint64 hash, const void* data, size_t size)
{
    const uint8* bytes = (const uint8*)data;
    size_t i;

    for ( i = 0; i < size; ++i ) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static inline XL6_TypeAbiId xl6TypeAbiIdText(const char* text)
{
    uint64 hash;

    if ( text == NULL || text[0] == '\0' ) {
        return XL6_TYPE_ABI_ID_INVALID;
    }
    hash = xl6TypeAbiIdBytes(
        UINT64_C(14695981039346656037),
        text,
        strlen(text)
    );
    return hash != 0 ? hash : UINT64_C(1);
}

#endif
