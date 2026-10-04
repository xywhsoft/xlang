#ifndef XL6_NULLABLE_STRING_H
#define XL6_NULLABLE_STRING_H
#include "src/runtime/xl6_text_value.h"
#include <string.h>

/* Stable physical ABI, not a source type name or a nominal language TypeId.
 * Descriptors crossing this trusted C boundary must be immutable and outlive
 * their storage. Matching the complete ABI keeps Sequence independent of the
 * optional resident factory: scalar-only programs do not select its code. */
#define XL6_NULLABLE_STRING_ID UINT64_C(0x514A294103E23D6D)
#define XL6_NULLABLE_STRING_ABI "xl6.nullable-string.v1"
static inline bool xl6NullableStringTypeAbiV1(const xrttype* type)
{
    return type && type->Id == XL6_NULLABLE_STRING_ID && type->Kind == XRT_TYPE_OPTIONAL &&
        type->Size == sizeof(xstrview) && type->Align == _Alignof(xstrview) &&
        type->Flags == (XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_NULLABLE|
                       XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE) &&
        type->AbiName.Data && type->AbiName.Size == sizeof(XL6_NULLABLE_STRING_ABI)-1u &&
        !memcmp(type->AbiName.Data,XL6_NULLABLE_STRING_ABI,sizeof(XL6_NULLABLE_STRING_ABI)-1u) &&
        type->Ops && type->Ops->Init && type->Ops->Copy && type->Ops->Move &&
        type->Ops->Drop && type->Ops->Clone && type->Ops->Compare && type->Ops->Hash;
}
#endif
