#include "src/runtime/xl6_tree_projection.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

xvalue* xl6TypedTreeProjectTakeV2(xtypedtree* tree, bool keys, xvalue* seed,
    uint64_t identity, uint64_t element, unsigned int transfer, size_t size,
    const xrttype* type, const xvalueconverter* projection,const XL6_TreeKeyCodecV1* codec)
{
    xl6_error_scope scope={0};
    /* A failed factory's existing outcome is part of this construction, not
     * an unrelated pre-call diagnostic. Keep its independent failure bit
     * even when diagnostic allocation failed. A valid seed starts normally. */
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) abort();
    XL6_Sequence* target=NULL;
    xtypedtreeiter iterator={0};
    bool started=false, success=false;
    const xrttype* sourceType=codec ? codec->type : keys ? type : xl6SequenceStorageTypeV1(transfer,type);
    if (!seed) goto done;
    if (!tree || !sourceType || (codec && (!keys || codec->abi!=XL6_TREE_BYTES_CODEC_ABI_V1 ||
            transfer!=3 || type!=xrtTypeValue())) || sourceType->Size!=size ||
        !xl6SequenceAdmitsArrayV1(seed,identity) ||
        (keys ? xrtTypedTreeKeyType(tree) : xrtTypedTreeValueType(tree))!=sourceType) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        goto done;
    }
    target=xl6SequenceBeginRoleV1(seed,XL6_SEQUENCE_ARRAY_V1,identity,element,transfer,size,type);
    if (!target) goto done;
    xtypedarray* storage=xl6SequenceStorageV1(target);
    if (xrtTypedArrayCount(storage)!=0) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    if (!xl6SequenceBindProjectionV1(target,projection)) goto done;
    size_t count=xrtTypedTreeCount(tree);
    if (xrtGetError()) goto done;
    if (count>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); goto done; }
    if (!xrtTypedTreeIterBegin(tree,&iterator)) goto done;
    started=true;
    if (!xrtTypedArrayReserve(storage,count)) goto done;
    const void* key=NULL;
    const void* value;
    while ((value=xrtTypedTreeIterNext(&iterator,&key))!=NULL) {
        const void* slot=keys ? key : value;
        /* The only custom key codec owns immutable byte atoms. Copying the
         * reference into a new Array slot preserves language value semantics;
         * no per-key staging allocation or mutable-shell alias is necessary. */
        if (!xl6SequenceAdmitSlotV1(target,slot) || !xrtTypedArrayPush(storage,slot)) goto done;
    }
    if (xrtGetError() || xrtTypedArrayCount(storage)!=count) goto done;
    success=true;
done:
    if (!success) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtTypedTreeIterEnd(&iterator);
    if (target) xl6SequenceEndV1(target);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtValueRelease(seed); seed=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return NULL; }
    return seed;
}
