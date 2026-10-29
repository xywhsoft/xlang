#include "src/runtime/xl6_dict_call_frame.h"
#include "src/runtime/xl6_call_frame_expansion_internal.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_array_admission.h"

bool xl6DictCallFrameExpandV1(uint64_t identity, uint64_t elementIdentity,
    const xvalue* tail, const xrtcallframe* fixed, xrtcallframe* output, void** state)
{
    if (!identity || !elementIdentity || !xl6FrameExpansionInputs(tail,fixed,output,state)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_FrameExpansion* owner=xrtCalloc(1,sizeof(*owner)); if (!owner) return false;
    XL6_DictLayoutV1 layout={0}; uint64_t cursor=0;
    int native=xl6DictErasedProbeV1(tail); if (native<0) goto fail;
    if (native) {
        XL6_SequenceAdmissionV3 policy={0};
        if (!xl6DictSchemaCopyV1(tail,&layout,&policy)) goto fail;
        bool valid=layout.identity==identity && layout.value.elementIdentity==elementIdentity &&
            xl6ReferenceAnyAdmissionV1(&policy,elementIdentity);
        xl6SequenceAdmissionDropV3(&policy);
        if (!valid || xrtGetError()) { if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE); goto fail; }
        if (xl6DictIteratorSnapshotErasedV1(tail,false,&owner->snapshot,&layout,&cursor)!=1) goto fail;
    } else {
        if (xrtValueType(tail)!=XVALUE_OBJECT || (xrtValueTypeId(tail) && xrtValueTypeId(tail)!=identity)) {
            xrtSetErrorKind(XERR_TYPE); goto fail;
        }
        if (!xrtValueIterBegin(tail,&owner->iterator)) goto fail;
    }
    if (!xl6FrameExpansionFixed(owner,fixed,0)) goto fail;
    for (;;) {
        if (!xl6FrameExpansionKeywordSlot(owner)) goto fail;
        size_t i=owner->keywordCount;
        if (native) {
            bool found=false;
            /* Private cursor publishes independently owned value and exact key. */
            if (!xl6DictIteratorAdvanceValueV1(owner->snapshot,&layout,cursor,
                &found,&owner->keywords[i],&owner->names[i])) goto fail;
            if (!found) break;
            ++owner->keywordCount;
        } else {
            xvaluekey key={0}; xvalue* value=NULL;
            xvalueiterresult status=xrtValueIterAdvance(&owner->iterator,&key,&value);
            if (status==XVALUE_ITER_END && !xrtGetError()) break;
            if (status!=XVALUE_ITER_ITEM || !value || key.Type!=XVALUE_KEY_STRING || xrtGetError()) {
                if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
                goto fail;
            }
            char* name=xrtStrDupN(key.String.Data,key.String.Size); if (!name) goto fail;
            owner->names[i]=(xstrview){name,key.String.Size}; ++owner->keywordCount;
            owner->keywords[i]=xrtValueRetain(value); if (!owner->keywords[i]) goto fail;
        }
    }
    xl6FrameExpansionPublish(owner,fixed,output,state); return true;
fail: return xl6FrameExpansionFailed(owner);
}
