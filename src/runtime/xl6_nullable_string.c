#include "src/runtime/xl6_nullable_string.h"
#include <stdlib.h>

/* Reuse XRT's owned view operations, including transactional copy and exact
 * embedded-NUL lengths. The only language distinction is presence: an absent
 * pointer is not a present empty string. No generated code owns this table. */
static bool init(ptr value, const xrttype* type)
{ (void)type; return xrtTypeInitValue(xrtTypeStringView(),value); }
static bool copy(ptr target, const void* source, const xrttype* type)
{ (void)type; return xrtTypeCopyValue(xrtTypeStringView(),target,source); }
static bool move(ptr target, ptr source, const xrttype* type)
{ (void)type; return xrtTypeMoveValue(xrtTypeStringView(),target,source); }
static void drop(ptr value, const xrttype* type)
{ (void)type; xrtTypeDropValue(xrtTypeStringView(),value); }
static int compare(const void* left, const void* right, const xrttype* type)
{
    (void)type; const xstrview* a=left; const xstrview* b=right; int result;
    if (!a->Data || !b->Data) return (a->Data != NULL) - (b->Data != NULL);
    if (!xrtTypeCompareValue(xrtTypeStringView(),a,b,&result)) abort();
    return result;
}
static uint64 hash(const void* value, const xrttype* type)
{
    (void)type; const xstrview* item=value; uint64 result;
    if (!item->Data) return 0;
    if (!xrtTypeHashValue(xrtTypeStringView(),item,&result)) abort();
    return result ^ UINT64_C(0x9E3779B97F4A7C15);
}
XL6_TEXT_API const xrttype* xl6NullableStringTypeV1(void)
{
    static const xrttypeops operations = {
        .Init=init,.Copy=copy,.Move=move,.Drop=drop,.Clone=copy,.Compare=compare,.Hash=hash
    };
    static const xrttype descriptor = {
        .Id=XL6_NULLABLE_STRING_ID,.Kind=XRT_TYPE_OPTIONAL,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_NULLABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("nullable<string>"),.AbiName=XRT_STR_INIT(XL6_NULLABLE_STRING_ABI),
        .Size=sizeof(xstrview),.Align=_Alignof(xstrview),.InstanceAlign=1,.Ops=&operations
    };
    return &descriptor;
}
XL6_TEXT_API bool xl6NullableStringEqualV1(xstrview left, xstrview right)
{
    if (!left.Data || !right.Data) return left.Data == right.Data;
    return xrtStrEqual(left,right);
}
/* xvalue uses its canonical immutable null sentinel. Present strings remain
 * separately owned values; empty and embedded-NUL strings never lose presence
 * or extent. XRT initializes/drops the destination around this callback. */
static bool toTyped(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    xstrview view = {0};
    (void)context;
    if (!xl6NullableStringTypeAbiV1(type)) { xrtSetErrorKind(XERR_TYPE); return false; }
    if (xrtValueType(source) != XVALUE_NULL && !xrtValueGetString(source,&view)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return xrtTypeCopyValue(xrtTypeStringView(),target,&view);
}
static xvalue* fromTyped(const xrttype* type, const void* source, ptr context)
{
    const xstrview* view = source;
    (void)context;
    if (!xl6NullableStringTypeAbiV1(type)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (!view->Data) {
        if (view->Size) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
        return xrtValueNull();
    }
    return xrtValueString(*view);
}
XL6_TEXT_API const xvalueconverter* xl6NullableStringConverterV1(void)
{
    static const xvalueconverter converter = {.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
