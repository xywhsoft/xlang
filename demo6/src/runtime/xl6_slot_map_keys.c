#include "xl6_slot_map.h"
#include "xl6_slot_map_key_value.h"
#include "src/api/xl_foreign_error.h"

xvalue* xl6SlotMapKeysV1(const xvalue* source,const XL6_SlotMapLayoutV1* layout,uint64_t identity)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    xvalue* witness=xl6SequenceSlotMapKeyCreateV1(XL6_SEQUENCE_ARRAY_V1,identity);
    xvalue* result=NULL;
    if (witness) result=xl6SlotMapSnapshotTakeV1(source,layout,true,witness,xl6SlotMapKeyConverterV1());
    if (!result || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) {
        xrtValueRelease(result); xl6ErrorScopePropagateFailure(outcome); return NULL;
    }
    return result;
}
