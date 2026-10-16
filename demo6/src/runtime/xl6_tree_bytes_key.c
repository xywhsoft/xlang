#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include <stdlib.h>
#include <string.h>

/* XRT byte Values are immutable atoms: language indexed writes replace the
 * binding with a new Value, not an edit of this atom. The existing Value-copy
 * contract retains that immutable atom in O(1), preserving independent language
 * values without copying its byte allocation. Inactive transfer slots are null;
 * the resident admission policy proves every present key is nonnull bytes. */
static bool init(ptr target,const xrttype* type)
{ (void)type;*(xvalue**)target=NULL;return true; }
static bool copy(ptr target,const void* source,const xrttype* type)
{ (void)type;return xrtTypeCopyValue(xrtTypeValue(),target,source); }
static bool move(ptr target,ptr source,const xrttype* type)
{ (void)type;return xrtTypeMoveValue(xrtTypeValue(),target,source); }
static void drop(ptr target,const xrttype* type)
{ (void)type;xrtTypeDropValue(xrtTypeValue(),target); }
static int compare(const void* left,const void* right,const xrttype* type)
{
    (void)type;xbytesview a={0},b={0};
    if(!xrtValueGetBytes(*(xvalue* const*)left,&a) || !xrtValueGetBytes(*(xvalue* const*)right,&b)) abort();
    size_t common=a.Size<b.Size ? a.Size : b.Size;
    int order=common ? memcmp(a.Data,b.Data,common) : 0;
    return order ? order : (a.Size>b.Size)-(a.Size<b.Size);
}
const XL6_TreeKeyCodecV1* xl6TreeBytesKeyCodecV1(void)
{
    static const xrttypeops ops={.Init=init,.Copy=copy,.Move=move,.Drop=drop,.Clone=copy,.Compare=compare};
    static const xrttype type={
        .Id=XL6_TREE_BYTES_CODEC_ABI_V1,.Kind=XRT_TYPE_HANDLE,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_REFERENCE|XRT_TYPE_FLAG_NULLABLE|
            XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("bytes-key"),.AbiName=XRT_STR_INIT("xl6.tree-bytes-key.v1"),
        .Size=sizeof(xvalue*),.Align=_Alignof(xvalue*),.InstanceAlign=1,.Ops=&ops
    };
    static const XL6_TreeKeyCodecV1 codec={XL6_TREE_BYTES_CODEC_ABI_V1,&type,xl6ReferenceBytesAdmissionV1};
    return &codec;
}
