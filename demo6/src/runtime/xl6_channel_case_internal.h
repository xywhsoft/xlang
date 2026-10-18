#ifndef XL6_CHANNEL_CASE_INTERNAL_H
#define XL6_CHANNEL_CASE_INTERNAL_H
#include "xl6_channel_case.h"
/* Stack-scoped preparation, never stored by XRT. All allocations precede the
 * atomic select. Input pins own real case references, NOT Channel activity
 * credits; the existing select provider acquires Channel credits itself. */
typedef struct XL6_ChannelOwnedSelectFrameV1 {
    size_t count;
    XL6_ChannelCaseV1* cases;
    XL6_ChannelCaseOwnerV1** pins;
    unsigned char* slots;
    XL6_ChannelSelectionOwnerV1* result;
    XL6_ChannelCaseV1 inlineCases[8];
    XL6_ChannelCaseOwnerV1* inlinePins[8];
    _Alignas(max_align_t) unsigned char inlineSlots[256];
} XL6_ChannelOwnedSelectFrameV1;
XL6_TEXT_API bool xl6ChannelOwnedSelectPrepareV1(XL6_ChannelCaseOwnerV1* const*,size_t,
    XL6_ChannelWaitOptionsV2*,XL6_ChannelOwnedSelectFrameV1*);
XL6_TEXT_API bool xl6ChannelOwnedSelectCompleteV1(XL6_ChannelOwnedSelectFrameV1*,XL6_ChannelSelectionV1);
XL6_TEXT_API XL6_ChannelSelectionOwnerV1* xl6ChannelOwnedSelectDiscardV1(XL6_ChannelOwnedSelectFrameV1*,bool);
#endif
