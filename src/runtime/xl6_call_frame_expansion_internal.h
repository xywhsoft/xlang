#ifndef XL6_CALL_FRAME_EXPANSION_INTERNAL_H
#define XL6_CALL_FRAME_EXPANSION_INTERNAL_H
#include "xl6_call_frame_expansion.h"
#include <stdint.h>

/* Resident-owned storage only: never capture producer callbacks/descriptors. */
typedef struct XL6_FrameExpansion {
    xvalue** arguments;
    size_t argumentCount;
    xvalue** keywords;
    xstrview* names;
    size_t keywordCount;
    size_t keywordCapacity;
    xvalue* snapshot;
    xvalueiter iterator;
} XL6_FrameExpansion;

static inline bool xl6FrameExpansionInputs(const xvalue* tail, const xrtcallframe* fixed,
    xrtcallframe* output, void** state)
{
    if (!tail || !fixed || !output || !state || *state ||
        (fixed->ArgumentCount && !fixed->Arguments) || fixed->KeywordCount) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
static inline bool xl6FrameExpansionFixed(XL6_FrameExpansion* owner,
    const xrtcallframe* fixed, size_t positional)
{
    if (fixed->ArgumentCount > SIZE_MAX-positional ||
        fixed->ArgumentCount+positional > SIZE_MAX/sizeof(xvalue*)) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    size_t count=fixed->ArgumentCount+positional;
    owner->arguments=count ? xrtCalloc(count,sizeof(xvalue*)) : NULL;
    if (count && !owner->arguments) return false;
    for (size_t i=0; i<fixed->ArgumentCount; ++i) {
        if (!fixed->Arguments[i]) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
        owner->arguments[i]=xrtValueRetain(fixed->Arguments[i]);
        if (!owner->arguments[i]) return false;
        ++owner->argumentCount;
    }
    return true;
}
/* Grow the two owning arrays transactionally. A failed growth leaves all
 * previous slots intact and available to the common cleanup transaction. */
static inline bool xl6FrameExpansionKeywordSlot(XL6_FrameExpansion* owner)
{
    if (owner->keywordCount < owner->keywordCapacity) return true;
    size_t limit=SIZE_MAX/sizeof(xvalue*);
    if (limit > SIZE_MAX/sizeof(xstrview)) limit=SIZE_MAX/sizeof(xstrview);
    if (owner->keywordCapacity >= limit) { xrtSetErrorKind(XERR_RANGE); return false; }
    size_t capacity=owner->keywordCapacity ? owner->keywordCapacity : 4;
    if (capacity > limit/2) capacity=limit; else capacity*=2;
    xvalue** values=xrtCalloc(capacity,sizeof(xvalue*));
    if (!values) return false;
    xstrview* names=xrtCalloc(capacity,sizeof(xstrview));
    if (!names) { xrtFree(values); return false; }
    for (size_t i=0; i<owner->keywordCount; ++i) {
        values[i]=owner->keywords[i]; names[i]=owner->names[i];
    }
    xrtFree(owner->keywords); xrtFree(owner->names);
    owner->keywords=values; owner->names=names; owner->keywordCapacity=capacity;
    return true;
}
static inline void xl6FrameExpansionPublish(XL6_FrameExpansion* owner,
    const xrtcallframe* fixed, xrtcallframe* output, void** state)
{
    *output=*fixed; output->ArgumentCount=owner->argumentCount;
    output->Arguments=owner->arguments; output->KeywordCount=owner->keywordCount;
    output->KeywordNames=owner->names; output->KeywordValues=owner->keywords;
    *state=owner;
}
static inline bool xl6FrameExpansionFailed(XL6_FrameExpansion* owner)
{
    void* failedOwner=owner;
    (void)xl6CallFrameExpansionFinishV1(&failedOwner,true); return false;
}
#endif
