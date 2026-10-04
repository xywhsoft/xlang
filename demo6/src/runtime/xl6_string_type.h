#ifndef XL6_STRING_TYPE_H
#define XL6_STRING_TYPE_H
#include "src/runtime/xl6_text_value.h"
#include <string.h>

/* Physical owned string slot. Unlike XRT's optional empty view, its default
 * is a present empty string. Moved-from/result slots may be zeroed, but a
 * stored value or a conversion source must have a non-NULL data pointer.
 * Sequence validates this ABI without linking the optional factory. */
#define XL6_STRING_ID UINT64_C(0x142F8251BF06A3F3)
#define XL6_STRING_ABI "xl6.string.v1"
static inline bool xl6StringTypeAbiV1(const xrttype* type)
{
    return type && type->Id == XL6_STRING_ID && type->Kind == XRT_TYPE_STRING &&
        type->Size == sizeof(xstrview) && type->Align == _Alignof(xstrview) &&
        type->Flags == (XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE) &&
        type->AbiName.Data && type->AbiName.Size == sizeof(XL6_STRING_ABI)-1u &&
        !memcmp(type->AbiName.Data,XL6_STRING_ABI,sizeof(XL6_STRING_ABI)-1u) &&
        type->Ops && type->Ops->Init && type->Ops->Copy && type->Ops->Move &&
        type->Ops->Drop && type->Ops->Clone && type->Ops->Compare && type->Ops->Hash;
}
#endif
