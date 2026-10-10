#include "xl6_list_value_iterator.h"
#include "xl6_value_iterator_internal.h"
#include "xl6_list.h"

static int create(const xvalue* source,XL6_ValueIteratorNativeSnapshotV1* output)
{
    XL6_ListLayoutV1 layout={0}; xvalue* cursor=NULL; uint64_t identity=0;
    int result=xl6ListIteratorSnapshotErasedV1(source,&cursor,&layout,&identity);
    if (result==1) *output=(XL6_ValueIteratorNativeSnapshotV1){
        cursor,layout.value,NULL,layout.identity,identity};
    return result;
}
static bool advance(const XL6_ValueIteratorNativeSnapshotV1* snapshot,
    bool* found,xvalue** item,xvaluekey* key)
{
    const XL6_ListLayoutV1 layout={snapshot->ownerIdentity,snapshot->layout};
    int64_t integer=0;
    bool ok=xl6ListIteratorAdvanceValueV1(snapshot->cursor,&layout,
        snapshot->cursorIdentity,found,item,&integer);
    if (ok && *found) { key->Type=XVALUE_KEY_INT; key->Integer=integer; }
    return ok;
}
static const XL6_ValueIteratorNativeOpsV1 ops={create,advance};
XL6_ValueIterator* xl6ListValueIteratorCreateV1(const xvalue* source)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    int native=xl6ListErasedProbeV1(source);
    if (native<0) return NULL;
    if (!native && xrtValueType(source)!=XVALUE_INT_MAP) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xl6ValueIteratorCreateResidentV1(source,&ops);
}
XL6_ValueIterator* xl6CollectionValueIteratorCreateV1(const xvalue* source)
{
    xvaluetype type=source ? xrtValueType(source) : XVALUE_NULL;
    if (type!=XVALUE_HANDLE && type!=XVALUE_ARRAY && type!=XVALUE_INT_MAP &&
        type!=XVALUE_SET && type!=XVALUE_OBJECT) {
        xrtSetErrorInfo(XERR_TYPE,"xlang",0,"for in value must be array, list, set or dict"); return NULL;
    }
    return xl6ValueIteratorCreateResidentV1(source,&ops);
}
