/* Intentionally instantiated once per selected physical profile; names and
 * layout are supplied by the tiny resident translation unit. No storage or
 * scalar algorithm is mirrored here. Init/copy/move/drop remain xrttype's
 * trivial operations. The absent payload and padding never affect equality
 * or hashing, and no descriptor points into unloadable generated code. */
#include "xl6_text_value.h"
#include <stdlib.h>

static int XL6_NULLABLE_COMPARE(const void* left, const void* right, const xrttype* type)
{
    const XL6_NULLABLE_CARRIER* a = left;
    const XL6_NULLABLE_CARRIER* b = right;
    int result;
    (void)type;
    if (!a->hasValue || !b->hasValue) return (int)a->hasValue - (int)b->hasValue;
    /* These are canonical builtin scalars: this operation is infallible and
     * allocation-free. Failure would be a violated trusted physical ABI. */
    if (!xrtTypeCompareValue(XL6_NULLABLE_ELEMENT(), &a->value, &b->value, &result)) abort();
    return result;
}
static uint64 XL6_NULLABLE_HASH(const void* value, const xrttype* type)
{
    const XL6_NULLABLE_CARRIER* item = value;
    uint64 hash;
    (void)type;
    if (!item->hasValue) return 0;
    if (!xrtTypeHashValue(XL6_NULLABLE_ELEMENT(), &item->value, &hash)) abort();
    return hash ^ UINT64_C(0x9E3779B97F4A7C15);
}
XL6_TEXT_API const xrttype* XL6_NULLABLE_GET(void)
{
    static const xrttypeops operations = {.Compare=XL6_NULLABLE_COMPARE,.Hash=XL6_NULLABLE_HASH};
    static const xrttype descriptor = {
        .Id=XL6_NULLABLE_ID,.Kind=XRT_TYPE_OPTIONAL,
        .Flags=XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP|
            XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_NULLABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT(XL6_NULLABLE_NAME),.AbiName=XRT_STR_INIT(XL6_NULLABLE_ABI),
        .Size=sizeof(XL6_NULLABLE_CARRIER),.Align=_Alignof(XL6_NULLABLE_CARRIER),
        .InstanceAlign=1,.Ops=&operations
    };
    return &descriptor;
}
#undef XL6_NULLABLE_CARRIER
#undef XL6_NULLABLE_ELEMENT
#undef XL6_NULLABLE_GET
#undef XL6_NULLABLE_COMPARE
#undef XL6_NULLABLE_HASH
#undef XL6_NULLABLE_NAME
#undef XL6_NULLABLE_ABI
#undef XL6_NULLABLE_ID
