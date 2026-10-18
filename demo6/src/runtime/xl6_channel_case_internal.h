#ifndef XL6_CHANNEL_CASE_INTERNAL_H
#define XL6_CHANNEL_CASE_INTERNAL_H
#include "xl6_channel_case.h"
/* Stable preparation, never stored by XRT. All allocations precede the
 * atomic select. Input pins own real case references, NOT Channel activity
 * credits; the select provider acquires Channel credits itself. Async owners
 * allocate this record BEFORE Prepare, never copy its self-referential inline
 * buffers, and Park all pins before native publication. */
typedef struct XL6_ChannelOwnedSelectFrameV1 {
    size_t count;
    XL6_ChannelCaseV1* cases;
    XL6_ChannelCaseOwnerV1** pins;
    unsigned char* slots;
    XL6_ChannelSelectionOwnerV1* result;
    bool parked,resultBorrowed;
    XL6_ChannelCaseV1 inlineCases[8];
    XL6_ChannelCaseOwnerV1* inlinePins[8];
    _Alignas(max_align_t) unsigned char inlineSlots[256];
} XL6_ChannelOwnedSelectFrameV1;
XL6_TEXT_API bool xl6ChannelOwnedSelectPrepareV1(XL6_ChannelCaseOwnerV1* const*,size_t,
    XL6_ChannelWaitOptionsV2*,XL6_ChannelOwnedSelectFrameV1*);
XL6_TEXT_API bool xl6ChannelOwnedSelectCompleteV1(XL6_ChannelOwnedSelectFrameV1*,XL6_ChannelSelectionV1);
XL6_TEXT_API XL6_ChannelSelectionOwnerV1* xl6ChannelOwnedSelectDiscardV1(XL6_ChannelOwnedSelectFrameV1*,bool);
XL6_TEXT_API bool xl6ChannelOwnedSelectParkV1(XL6_ChannelOwnedSelectFrameV1*);
XL6_TEXT_API bool xl6ChannelOwnedSelectTraceV1(const XL6_ChannelOwnedSelectFrameV1*,xrtownershipvisitor,ptr);
/* Private prepublication result handoff. The returned owned handle is the
 * ONE creator owner of the preallocated selection. The frame borrows it for
 * Complete and MUST be discarded before that handle is released. Trace then
 * omits the borrowed result; the containing producer traces its actual handle
 * or class payload instead. Failure consumes the result but leaves the whole
 * preparation safe to Discard(false). No allocation is needed at completion.
 * Discard returns a creator only when still owned directly by the frame. */
XL6_TEXT_API xvalue* xl6ChannelOwnedSelectResultHandleV1(XL6_ChannelOwnedSelectFrameV1*);
#endif
