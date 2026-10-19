#ifndef XL6_CHANNEL_H
#define XL6_CHANNEL_H
#include "xl6_sequence.h"
#include "xl6_channel_abi.h"

/* Resident typed ownership over XRT's opaque-pointer channel. This is the
 * native foundation, not the public WaitQueue/Channel language cutover.
 * A consumed admitted Array/Stack supplies immutable resident metadata. No
 * source-module callback, schema storage or language output borrow is kept.
 * Owned DIRECT slots reuse transfers 1..5 (trivial, exact string, retained
 * xvalue, native Future reference, resident native leaf record), with complete
 * admission and graph duties. Invocation/boxed codecs are separate contracts,
 * not authorized merely because their representation is also a pointer.
 * Capacity is exact; zero uses XRT rendezvous, not a hidden minimum capacity. */
XL6_TEXT_API bool xl6ChannelAdmitsSchemaV1(const xvalue*,uint64_t,
    const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
/* Capture immutable resident metadata without opening an operation or pinning
 * the Channel BUSY. Both outputs start empty and stay unchanged on failure.
 * The caller owns one admission reference; no channel/storage/code borrow is
 * retained. Concurrent waits are permitted; retirement is still refused. */
XL6_TEXT_API bool xl6ChannelSchemaCopyV1(const xvalue*,uint64_t,
    XL6_SequenceLayoutV1*,XL6_SequenceAdmissionV3*);
/* bool reports actual operation/cleanup failure independently of allocating
 * an error. Status is only published on normal flow; absence leaves the fresh
 * private output slot unchanged. OK with a NULL nullable payload is distinct
 * from EMPTY/CLOSED/TIMEOUT. Receive moves ownership without copying/allocating.
 * A managed target must be empty, with exact size/alignment and no overlap with
 * the status slot. This is a private result acquisition slot, never byref.
 * The caller owns it on EVERY path, including failure after a committed move.
 * timeoutMs must be zero outside FOR; FOR rejects negatives and finite
 * overflow instead of converting it into the native infinite sentinel.
 * The absolute deadline is fixed before copying a message; copy/allocation
 * time is part of the timeout. Send does not consume its input. */
/* Close may run concurrently with waits; it wakes them and preserves queued
 * ownership for draining. An active operation pins the native owner. Graph
 * observation refuses BUSY instead of waiting under the frozen world gate.
 * Closing explicitly is required to end indefinite application-level waits. */
XL6_TEXT_API const xrtownershipadapterv1* xl6ChannelOwnershipAdapterV1(xrtownershipref);
#endif
