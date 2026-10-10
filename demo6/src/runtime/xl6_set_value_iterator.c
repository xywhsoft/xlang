#include "xl6_set_value_iterator.h"
#include "xl6_value_iterator_internal.h"
#include "xl6_set.h"

static int create(const xvalue* source,XL6_ValueIteratorNativeSnapshotV1* output)
{
    XL6_SetLayoutV1 layout={0}; xvalue* cursor=NULL; uint64_t identity=0;
    int result=xl6SetIteratorSnapshotErasedV1(source,false,&cursor,&layout,&identity);
    if (result==1) *output=(XL6_ValueIteratorNativeSnapshotV1){
        cursor,layout.value,NULL,layout.identity,identity,XL6_VALUE_ITERATOR_SET_V1};
    return result;
}
static bool advance(const XL6_ValueIteratorNativeSnapshotV1* snapshot,
    bool* found,xvalue** item,xvaluekey* key)
{
    const XL6_SetLayoutV1 layout={snapshot->ownerIdentity,snapshot->layout};
    bool ok=xl6SetIteratorAdvanceValueV1(snapshot->cursor,&layout,
        snapshot->cursorIdentity,found,item);
    /* Set iteration has no index/key. The shared carrier supplies ordinals
     * separately; do not manufacture an Array or List key. */
    if (ok && *found) *key=(xvaluekey){0};
    return ok;
}
static const XL6_ValueIteratorNativeOpsV1 ops={create,advance};
const XL6_ValueIteratorNativeOpsV1* xl6SetValueIteratorNativeOpsV1Get(void)
{ return &ops; }
XL6_ValueIterator* xl6SetValueIteratorCreateCheckedV1(const xvalue* source)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    int native=xl6SetErasedProbeV1(source);
    if (native<0) return NULL;
    if (!native && xrtValueType(source)!=XVALUE_SET) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xl6ValueIteratorCreateResidentV1(source,&ops);
}
