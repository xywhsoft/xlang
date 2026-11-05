#ifndef XL6_CHANNEL_WAIT_ABI_H
#define XL6_CHANNEL_WAIT_ABI_H
#include "xl6_channel_abi.h"
typedef struct xcancel xcancel;
/* Native-only foundation until the corresponding closed source IR cutover.
 * Slots/case arrays are private synchronous acquisition storage, NOT language
 * byref. No pointer to these descriptors survives the call. */
typedef struct XL6_ChannelWaitOptionsV2 {
    size_t size;
    unsigned mode; /* TRY/WAIT/FOR from V1, or UNTIL below. */
    int64_t timeoutMs;
    uint64_t deadlineMs; /* Same monotonic millisecond clock as xl6MonotonicMillisecondsV1. */
} XL6_ChannelWaitOptionsV2;
enum { XL6_CHANNEL_UNTIL_V2=3 };
enum { XL6_CHANNEL_CASE_SEND_V1=0, XL6_CHANNEL_CASE_RECEIVE_V1=1 };
typedef struct XL6_ChannelCaseV1 {
    const xvalue* channel;
    uint64_t identity,element;
    const xrttype* type;
    XL6_ChannelSlotV1 slot;
    unsigned operation;
} XL6_ChannelCaseV1;
typedef struct XL6_ChannelSelectionV1 {
    size_t index; /* SIZE_MAX on timeout/cancellation; closed has a case index. */
    XL6_ChannelStatusV1 status;
} XL6_ChannelSelectionV1;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CHANNEL_WAIT_API __declspec(dllexport)
#else
#define XL6_CHANNEL_WAIT_API
#endif
XL6_CHANNEL_WAIT_API bool xl6ChannelSendWaitV2(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitOptionsV2,XL6_ChannelStatusV1*);
XL6_CHANNEL_WAIT_API bool xl6ChannelReceiveWaitV2(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitOptionsV2,XL6_ChannelStatusV1*);
XL6_CHANNEL_WAIT_API bool xl6ChannelSendCancelV2(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitOptionsV2,xcancel*,XL6_ChannelStatusV1*);
XL6_CHANNEL_WAIT_API bool xl6ChannelReceiveCancelV2(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitOptionsV2,xcancel*,XL6_ChannelStatusV1*);
XL6_CHANNEL_WAIT_API bool xl6ChannelSelectV1(const XL6_ChannelCaseV1*,size_t,
    XL6_ChannelWaitOptionsV2,XL6_ChannelSelectionV1*);
XL6_CHANNEL_WAIT_API bool xl6ChannelSelectCancelV1(const XL6_ChannelCaseV1*,size_t,
    XL6_ChannelWaitOptionsV2,xcancel*,XL6_ChannelSelectionV1*);
#undef XL6_CHANNEL_WAIT_API
/* Invalid input / resource or cleanup failure returns false independently of
 * diagnostic allocation and does not publish status/index. After a committed
 * receive the caller owns its private slot EVEN if later cleanup fails.
 * Send inputs are copied, not consumed; unselected copies are always released.
 * Ready operations win over cancellation/expiration, matching pinned XRT.
 * Non-ready TRY maps timeout to EMPTY/FULL for single operations; Select TRY
 * uses TIMEOUT + SIZE_MAX. Cancellation is a normal distinct CANCELLED status.
 * FOR rejects negative/overflow; UNTIL rejects the UINT64_MAX infinity sentinel;
 * WAIT is explicit infinity. Unused option fields must be zero.
 * Selection is heterogeneous and atomic, not repeated single-channel polling.
 * Source values/cases must remain stable during this synchronous call. */
#endif
