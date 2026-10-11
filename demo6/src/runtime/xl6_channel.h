#ifndef XL6_CHANNEL_H
#define XL6_CHANNEL_H
#include "xl6_sequence.h"

/* Resident typed ownership over XRT's opaque-pointer channel. This is the
 * native foundation, not the public WaitQueue/Channel language cutover.
 * A consumed admitted Array/Stack supplies immutable resident metadata. No
 * source-module callback, schema storage or language output borrow is kept.
 * V1 admits transfers 1..3 (trivial, exact string, retained xvalue reference).
 * Other transfers are checked refusals, not silently degraded representations.
 * Capacity is exact; zero uses XRT rendezvous, not a hidden minimum capacity. */
typedef enum XL6_ChannelStatusV1 {
    XL6_CHANNEL_OK_V1=0, XL6_CHANNEL_EMPTY_V1=1, XL6_CHANNEL_FULL_V1=2,
    XL6_CHANNEL_CLOSED_V1=3, XL6_CHANNEL_TIMEOUT_V1=4
} XL6_ChannelStatusV1;
typedef enum XL6_ChannelWaitV1 {
    XL6_CHANNEL_TRY_V1=0, XL6_CHANNEL_WAIT_V1=1, XL6_CHANNEL_FOR_V1=2
} XL6_ChannelWaitV1;
typedef struct XL6_ChannelSlotV1 { void* data; size_t size; } XL6_ChannelSlotV1;
enum {
    XL6_CHANNEL_QUERY_COUNT_V1=0, XL6_CHANNEL_QUERY_CAPACITY_V1=1,
    XL6_CHANNEL_QUERY_CLOSED_V1=2, XL6_CHANNEL_QUERY_DRAINED_V1=3
};
XL6_TEXT_API xvalue* xl6ChannelCreateTakeV1(uint64_t, xvalue*, int64_t);
XL6_TEXT_API bool xl6ChannelAdmitsSchemaV1(const xvalue*,uint64_t,
    const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
/* bool reports actual operation/cleanup failure independently of allocating
 * an error. Status is only published on normal flow; absence leaves the fresh
 * private output slot unchanged. OK with a NULL nullable payload is distinct
 * from EMPTY/CLOSED/TIMEOUT. Receive moves ownership without copying/allocating.
 * A managed target must be empty, with exact size/alignment and no overlap with
 * the status slot. This is a private result acquisition slot, never byref.
 * The caller owns it on EVERY path, including failure after a committed move.
 * timeoutMs must be zero outside FOR; FOR rejects negatives and saturates a
 * positive conversion to XRT microseconds. Send does not consume its input. */
XL6_TEXT_API bool xl6ChannelSendV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_TEXT_API bool xl6ChannelReceiveV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_TEXT_API int64_t xl6ChannelQueryV1(const xvalue*,uint64_t,unsigned int);
/* Close may run concurrently with waits; it wakes them and preserves queued
 * ownership for draining. An active operation pins the native owner. Graph
 * observation refuses BUSY instead of waiting under the frozen world gate.
 * Closing explicitly is required to end indefinite application-level waits. */
XL6_TEXT_API bool xl6ChannelCloseV1(const xvalue*,uint64_t);
XL6_TEXT_API const xrtownershipadapterv1* xl6ChannelOwnershipAdapterV1(xrtownershipref);
#endif
