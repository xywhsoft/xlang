#ifndef XL6_TEXT_THREAD_H
#define XL6_TEXT_THREAD_H
#include "xl6_text_internal.h"

#if !defined(XRT_SINGLE_HEADER) || defined(XRT_FEATURE_THREAD)
/* Keep managed creator identity alive rather than trusting recyclable OS IDs.
 * External host threads must remain alive while their mutable resource is used.
 * This is an identity check, not a mutex or permission for reentrant mutation. */
typedef struct XL6_TextThreadOwner {
    xthread* managed;
    uint64_t external;
} XL6_TextThreadOwner;
static inline bool xl6TextThreadInit(XL6_TextThreadOwner* owner)
{
    xthread* current=xrtThreadCurrent();
    if (current) {owner->managed=xrtThreadRef(current);return owner->managed!=NULL;}
    owner->external=xrtThreadCurrentId();return true;
}
static inline bool xl6TextThreadCheck(const XL6_TextThreadOwner* owner)
{
    xthread* current=xrtThreadCurrent();
    if (owner->managed ? current==owner->managed : !current && owner->external==xrtThreadCurrentId()) return true;
    xrtSetErrorKind(XERR_STATE);return false;
}
static inline void xl6TextThreadDrop(XL6_TextThreadOwner* owner)
{xrtThreadDestroy(owner->managed);owner->managed=NULL;}
#endif
#endif
