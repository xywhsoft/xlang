#include "xl6_slot_map.h"
#include "xl6_slot_map_key_value.h"
#include "src/api/xl_foreign_error.h"

bool xl6SlotMapPairsV1(const xvalue* source,const XL6_SlotMapLayoutV1* layout,uint64_t keyArrayIdentity,
    const xvalueconverter* converter,XL6_SlotMapSnapshotPairV1* output)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    XL6_SlotMapSnapshotPairV1 prepared={0}; bool success=false;
    if(!output || output->keys || output->values) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvalue* witness=xl6SequenceSlotMapKeyCreateV1(XL6_SEQUENCE_ARRAY_V1,keyArrayIdentity);
        if(witness) success=xl6SlotMapSnapshotPairTakeV1(source,layout,witness,xl6SlotMapKeyConverterV1(),converter,&prepared);
    }
    if(!success || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok) {
        xrtValueRelease(prepared.keys); xrtValueRelease(prepared.values);
        xl6ErrorScopePropagateFailure(outcome); return false;
    }
    *output=prepared; return true;
}
