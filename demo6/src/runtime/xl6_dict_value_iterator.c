#include "xl6_dict_value_iterator.h"
#include "xl6_value_iterator_internal.h"
#include "xl6_dict.h"

static int create(const xvalue* source,XL6_ValueIteratorNativeSnapshotV1* output)
{
    XL6_DictLayoutV1 layout={0}; xvalue* cursor=NULL; uint64_t identity=0;
    int result=xl6DictIteratorSnapshotErasedV1(source,false,&cursor,&layout,&identity);
    if (result==1) *output=(XL6_ValueIteratorNativeSnapshotV1){
        cursor,layout.value,NULL,layout.identity,identity,XL6_VALUE_ITERATOR_DICT_V1};
    return result;
}
static bool advance(const XL6_ValueIteratorNativeSnapshotV1* snapshot,
    bool* found,xvalue** item,xvaluekey* key)
{
    const XL6_DictLayoutV1 layout={snapshot->ownerIdentity,snapshot->layout}; xstrview text={0};
    bool ok=xl6DictIteratorAdvanceViewV1(snapshot->cursor,&layout,snapshot->cursorIdentity,found,item,&text);
    if (ok && *found) { key->Type=XVALUE_KEY_STRING; key->String=text; }
    return ok;
}
static const XL6_ValueIteratorNativeOpsV1 ops={create,advance};
XL6_ValueIterator* xl6DictValueIteratorCreateCheckedV1(const xvalue* source)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    int native=xl6DictErasedProbeV1(source); if (native<0) return NULL;
    if (!native && xrtValueType(source)!=XVALUE_OBJECT) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xl6ValueIteratorCreateResidentV1(source,&ops);
}
const XL6_ValueIteratorNativeOpsV1* xl6DictValueIteratorNativeOpsV1Get(void) { return &ops; }
