#include "xl6_slot_map_iterator_internal.h"
#include "xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"

static bool finish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static bool same(const XL6_SequenceLayoutV1* a,const XL6_SequenceLayoutV1* b)
{
    return a && b && a->identity==b->identity && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type;
}
static void discard(XL6_ValueIteratorNativePairSnapshotV1* snapshot,xl6_error_scope* scope)
{
    xvalue* owned[2]={snapshot->keys,snapshot->values};
    *snapshot=(XL6_ValueIteratorNativePairSnapshotV1){0};
    xerror* prior=scope ? NULL : xrtTakeError();
    for(unsigned i=0;i<2;++i) if(owned[i]) {
        xrtValueRelease(owned[i]);
        if(scope && (xrtGetError() || scope->failed)) xl6ErrorScopeCaptureFailure(scope);
    }
    if(!scope) { xrtClearError(); xrtSetErrorTake(prior); }
}
static XL6_Sequence* begin(const xvalue* value,const XL6_SequenceLayoutV1* layout)
{
    return xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,layout->identity,
        layout->elementIdentity,layout->transfer,layout->size,layout->type);
}
static bool capture(const xvalue* value,const XL6_SequenceLayoutV1* expected,
    XL6_SequenceLayoutV1* layout,const xvalueconverter** projection,size_t* count)
{
    if (!expected) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* pinned=NULL;
    int status=xl6SequenceBeginErasedArrayV1(value,&pinned,layout,projection);
    if (status!=1) { if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE); return false; }
    bool success=same(layout,expected) && (!*projection || !(*projection)->Context);
    if (success) *count=xrtTypedArrayCount(xl6SequenceStorageV1(pinned));
    else xrtSetErrorKind(XERR_TYPE);
    xl6SequenceEndV1(pinned); return success;
}
XL6_ValueIterator* xl6SlotMapIteratorWrapTakeV1(XL6_SlotMapSnapshotPairV1* input,
    const XL6_SequenceLayoutV1* keyLayout,const XL6_SequenceLayoutV1* valueLayout,
    size_t count,const XL6_ValueIteratorNativePairOpsV1* ops)
{
    if (!input) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_ValueIteratorNativePairSnapshotV1 snapshot={.keys=input->keys,.values=input->values};
    *input=(XL6_SlotMapSnapshotPairV1){0}; xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        discard(&snapshot,NULL); return NULL;
    }
    XL6_ValueIterator* result=NULL; bool success=true; size_t keys=0,values=0;
    const xvalueconverter* keyProjection=NULL;
    if (snapshot.keys) {
        snapshot.mode|=XL6_VALUE_ITERATOR_PAIR_KEY_V1;
        success=capture(snapshot.keys,keyLayout,&snapshot.keyLayout,&keyProjection,&keys);
    }
    if (success && snapshot.values) {
        snapshot.mode|=XL6_VALUE_ITERATOR_PAIR_ITEM_V1;
        success=capture(snapshot.values,valueLayout,&snapshot.valueLayout,&snapshot.valueProjection,&values);
    }
    if (success) {
        if (count==SIZE_MAX) count=snapshot.keys ? keys : snapshot.values ? values : SIZE_MAX;
        success=count<=(size_t)INT64_MAX && (!snapshot.keys || keys==count) &&
            (!snapshot.values || values==count) && (!snapshot.keys || snapshot.keys!=snapshot.values);
        if (!success) xrtSetErrorKind(XERR_STATE);
    }
    if (success) { snapshot.count=count; result=xl6ValueIteratorCreatePairTakeV1(&snapshot,ops); success=result!=NULL; }
    else {
        /* Capture the actual admission/shape error before releasing owners;
         * do not manufacture a different invalid-ops argument error. */
        xl6ErrorScopeCaptureFailure(&scope); discard(&snapshot,&scope);
    }
    if (!finish(&scope,success)) {
        (xl6ValueIteratorRuntimeV2Get()->Destroy)(result); return NULL;
    }
    return result;
}
bool xl6SlotMapIteratorAdvanceSnapshotV1(const XL6_ValueIteratorNativePairSnapshotV1* snapshot,
    size_t ordinal,bool* found,xvalue** item)
{
    if (found) *found=false;
    if (item) *item=NULL;
    if (!snapshot || !found || !item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    bool success=ordinal<=snapshot->count; xvalue* prepared=NULL;
    XL6_Sequence* keys=NULL; XL6_Sequence* values=NULL;
    if (!success) xrtSetErrorKind(XERR_STATE);
    if (success && snapshot->keys) {
        keys=begin(snapshot->keys,&snapshot->keyLayout);
        success=keys && xrtTypedArrayCount(xl6SequenceStorageV1(keys))==snapshot->count;
    }
    if (success && snapshot->values) {
        values=begin(snapshot->values,&snapshot->valueLayout);
        success=values && xrtTypedArrayCount(xl6SequenceStorageV1(values))==snapshot->count;
    }
    if (!success && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    bool available=success && ordinal<snapshot->count;
    if (available && values) {
        prepared=xl6SequenceProjectionTakeV1(values,
            xrtTypedArrayGetValue(xl6SequenceStorageV1(values),ordinal,snapshot->valueProjection));
        success=prepared!=NULL;
    }
    if (values) xl6SequenceEndV1(values);
    if (keys) xl6SequenceEndV1(keys);
    if (!finish(&scope,success)) {
        xerror* error=xrtTakeError(); xrtValueRelease(prepared);
        xrtClearError(); xrtSetErrorTake(error); return false;
    }
    *found=available; *item=prepared; return true;
}
const XL6_ValueIteratorNativePairOpsV1* xl6SlotMapItemIteratorOpsV1Get(void)
{
    static const XL6_ValueIteratorNativePairOpsV1 ops={xl6SlotMapIteratorAdvanceSnapshotV1,NULL};
    return &ops;
}
