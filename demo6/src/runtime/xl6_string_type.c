#include "src/runtime/xl6_string_type.h"
#include <stdlib.h>

static bool present(const void* value)
{
    if (!((const xstrview*)value)->Data) { xrtSetErrorKind(XERR_TYPE); return false; }
    return true;
}
static bool init(ptr value, const xrttype* type)
{
    (void)type;
    /* Each initialized slot owns its allocation, including zero-length text.
     * No static sentinel can escape through Take/Move and reach xrtFree. */
    xstrview item={0};
    *(xstrview*)value=item;
    item.Data=xrtStrDupN("",0);
    if (!item.Data) return false;
    *(xstrview*)value=item; return true;
}
static bool copy(ptr target, const void* source, const xrttype* type)
{ (void)type; return present(source) && xrtTypeCopyValue(xrtTypeStringView(),target,source); }
static bool move(ptr target, ptr source, const xrttype* type)
{ (void)type; return present(source) && xrtTypeMoveValue(xrtTypeStringView(),target,source); }
static void drop(ptr value, const xrttype* type)
{ (void)type; xrtTypeDropValue(xrtTypeStringView(),value); }
static int compare(const void* left, const void* right, const xrttype* type)
{
    (void)type; int result;
    if (!xrtTypeCompareValue(xrtTypeStringView(),left,right,&result)) abort();
    return result;
}
static uint64 hash(const void* value, const xrttype* type)
{
    (void)type; uint64 result;
    if (!xrtTypeHashValue(xrtTypeStringView(),value,&result)) abort();
    return result;
}
XL6_TEXT_API const xrttype* xl6StringTypeV1(void)
{
    static const xrttypeops operations={
        .Init=init,.Copy=copy,.Move=move,.Drop=drop,.Clone=copy,.Compare=compare,.Hash=hash
    };
    static const xrttype descriptor={
        .Id=XL6_STRING_ID,.Kind=XRT_TYPE_STRING,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("string"),.AbiName=XRT_STR_INIT(XL6_STRING_ABI),
        .Size=sizeof(xstrview),.Align=_Alignof(xstrview),.InstanceAlign=1,.Ops=&operations
    };
    return &descriptor;
}
