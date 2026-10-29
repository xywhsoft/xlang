#include "xl6_slot_map_iterator_internal.h"
#include "src/type/xl6_native_value_ids.h"
#include "src/api/xl_foreign_error.h"

XL6_SequenceLayoutV1 xl6SlotMapIteratorKeyLayoutV1(uint64_t identity)
{
    return (XL6_SequenceLayoutV1){identity,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,
        XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1()};
}
static bool copyKey(const XL6_ValueIteratorNativePairSnapshotV1* snapshot,
    size_t ordinal,XL6_SlotMapKeyV1* output)
{
    if (!snapshot || !output || output->instance || output->slot) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    const XL6_SequenceLayoutV1* layout=&snapshot->keyLayout; XL6_SlotMapKeyV1 prepared={0};
    XL6_Sequence* pinned=NULL; bool success=false;
    if (!(snapshot->mode&XL6_VALUE_ITERATOR_PAIR_KEY_V1) || !snapshot->keys || ordinal>=snapshot->count ||
        layout->elementIdentity!=XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID ||
        layout->transfer!=XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 || layout->size!=sizeof(prepared) ||
        layout->type!=xl6SlotMapKeyTypeV1()) xrtSetErrorKind(XERR_STATE);
    else {
        pinned=xl6SequenceBeginRoleV1(snapshot->keys,XL6_SEQUENCE_ARRAY_V1,layout->identity,
            layout->elementIdentity,layout->transfer,layout->size,layout->type);
        if (pinned && xrtTypedArrayCount(xl6SequenceStorageV1(pinned))==snapshot->count) {
            const XL6_SlotMapKeyV1* key=xrtTypedArrayConstGet(xl6SequenceStorageV1(pinned),ordinal);
            if (key && key->instance && key->slot) success=xrtTypeCopyValue(layout->type,&prepared,key);
        }
    }
    if (pinned) xl6SequenceEndV1(pinned);
    if (!success || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) {
        xl6ErrorScopePropagateFailure(outcome);
        xerror* error=xrtTakeError(); xl6SlotMapKeyDropValueV1(prepared);
        xrtClearError(); xrtSetErrorTake(error); return false;
    }
    *output=prepared; return true;
}
const XL6_ValueIteratorNativePairOpsV1* xl6SlotMapKeyIteratorOpsV1Get(void)
{
    static const XL6_ValueIteratorNativePairOpsV1 ops={xl6SlotMapIteratorAdvanceSnapshotV1,copyKey};
    return &ops;
}
