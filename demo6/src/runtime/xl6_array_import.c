#include "src/runtime/xl6_array_import.h"
#include "src/runtime/xl6_array_input.h"
#include <limits.h>

static void releasePreservingError(xvalue* value)
{
    xerror* prior=xrtTakeError();
    xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}

/* -1 recognized failure, 0 explicit conversion required, 1 copied. Never
 * infer native compatibility from a bound TypeId or a descriptor name. */
static int copyNative(XL6_Sequence* target,const XL6_SequenceLayoutV1* layout,
    const xvalueconverter* converter,const xvalue* source)
{
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 sourceLayout;
    const xvalueconverter* sourceConverter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&sourceLayout,&sourceConverter);
    if (status<=0) return status;
    bool compatible=sourceLayout.elementIdentity==layout->elementIdentity &&
        sourceLayout.transfer==layout->transfer && sourceLayout.size==layout->size &&
        sourceLayout.type==layout->type && sourceConverter==converter &&
        xl6SequenceAdmissionEquivalentV1(target,owner);
    int result=0;
    if (compatible) {
        xtypedarray* storage=xl6SequenceStorageV1(owner);
        if (xrtTypedArrayCount(storage)>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); result=-1; }
        else if (xrtTypedArrayAppend(xl6SequenceStorageV1(target),storage)) result=1;
        else { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); result=-1; }
    }
    xl6SequenceEndV1(owner); return result;
}

xvalue* xl6ArrayImportTakeV1(xvalue* seed, const xvalue* source)
{
    XL6_Sequence* owner=NULL;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    XL6_ArrayInputV1 input={0};
    bool ok=false;
    if (!seed || !source || seed==source) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    int status=xl6SequenceBeginErasedArrayV1(seed,&owner,&layout,&converter);
    if (status<=0) {
        if (!status) xrtSetErrorKind(XERR_TYPE);
        goto done;
    }
    /* Projection binding alone is not proof of a logical element domain.
     * Take an owned policy witness rather than inspecting opaque owner data. */
    XL6_SequenceAdmissionV3 admission={0};
    if (!xl6SequenceAdmissionCopyV3(owner,&admission)) goto done;
    bool admitted=admission.ops!=NULL;
    xl6SequenceAdmissionDropV3(&admission);
    xtypedarray* storage=xl6SequenceStorageV1(owner);
    if (!admitted || xrtTypedArrayCount(storage)!=0) {
        xrtSetErrorKind(XERR_STATE); goto done;
    }
    int copied=copyNative(owner,&layout,converter,source);
    if (copied) { ok=copied>0; goto done; }
    /* Keep the destination pin through all source conversions and cleanup.
     * A converter cannot publish/mutate/retire the construction seed. */
    if (!xl6ArrayInputAcquireV1(source,&input)) goto done;
    if (!xrtTypedArrayReserve(storage,input.count)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    for (size_t index=0;index<input.count;++index) {
        if (!xl6SequenceAdmitValueV1(owner,input.items[index])) goto done;
        if (!xrtTypedArrayPushValue(storage,input.items[index],converter)) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
            goto done;
        }
    }
    ok=true;
done:
    xl6ArrayInputReleaseV1(&input);
    if (owner) xl6SequenceEndV1(owner);
    if (!ok) { releasePreservingError(seed); return NULL; }
    return seed;
}
