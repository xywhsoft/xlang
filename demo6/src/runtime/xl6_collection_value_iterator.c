#include "xl6_list_value_iterator.h"
#include "xl6_set_value_iterator.h"
#include "xl6_value_iterator_internal.h"

/* Erased dispatch is its own component. Static List/Array/Set iteration
 * links only its own family; no global callback registry or monolithic
 * iterator dependency silently pulls in unrelated native containers. */
static int create(const xvalue* source,XL6_ValueIteratorNativeSnapshotV1* output)
{
    int result=xl6ListValueIteratorNativeOpsV1Get()->Create(source,output);
    return result ? result : xl6SetValueIteratorNativeOpsV1Get()->Create(source,output);
}
static bool advance(const XL6_ValueIteratorNativeSnapshotV1* snapshot,
    bool* found,xvalue** item,xvaluekey* key)
{
    switch (snapshot->family) {
    case XL6_VALUE_ITERATOR_LIST_V1:
        return xl6ListValueIteratorNativeOpsV1Get()->Advance(snapshot,found,item,key);
    case XL6_VALUE_ITERATOR_SET_V1:
        return xl6SetValueIteratorNativeOpsV1Get()->Advance(snapshot,found,item,key);
    default: xrtSetErrorKind(XERR_STATE); return false;
    }
}
static const XL6_ValueIteratorNativeOpsV1 ops={create,advance};
XL6_ValueIterator* xl6CollectionValueIteratorCreateV1(const xvalue* source)
{
    xvaluetype type=source ? xrtValueType(source) : XVALUE_NULL;
    if (type!=XVALUE_HANDLE && type!=XVALUE_ARRAY && type!=XVALUE_INT_MAP &&
        type!=XVALUE_SET && type!=XVALUE_OBJECT) {
        xrtSetErrorInfo(XERR_TYPE,"xlang",0,"for in value must be array, list, set or dict"); return NULL;
    }
    return xl6ValueIteratorCreateResidentV1(source,&ops);
}
