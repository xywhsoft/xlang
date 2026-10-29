#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include <stdlib.h>

/* XRT has no char type kind. This closed physical record deliberately avoids
 * UNSIGNED_INT: generic typed-value conversion must not turn a Unicode scalar
 * into XVALUE_UINT or accept an arbitrary integer as XVALUE_CHAR. */
static int compare(const void* left, const void* right, const xrttype* type)
{
    (void)type; int result;
    if (!xrtTypeCompareValue(xrtTypeUInt32(),left,right,&result)) abort();
    return result;
}
static uint64 hash(const void* value, const xrttype* type)
{
    (void)type; uint64 result;
    if (!xrtTypeHashValue(xrtTypeUInt32(),value,&result)) abort();
    return result;
}
XL6_TEXT_API const xrttype* xl6CharTypeV1(void)
{
    static const xrttypeops operations={.Compare=compare,.Hash=hash};
    static const xrttype descriptor={
        .Id=XL6_CHAR_ID,.Kind=XRT_TYPE_RECORD,
        .Flags=XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP|
            XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("char"),.AbiName=XRT_STR_INIT(XL6_CHAR_ABI),
        .Size=sizeof(uint32_t),.Align=_Alignof(uint32_t),.InstanceAlign=1,.Ops=&operations
    };
    return &descriptor;
}
