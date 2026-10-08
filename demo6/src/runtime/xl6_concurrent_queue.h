#ifndef XL6_CONCURRENT_QUEUE_H
#define XL6_CONCURRENT_QUEUE_H
#include "src/runtime/xl6_sequence.h"

/* Private resident transport. A consumed Array/Stack schema supplies the full
 * owned admission and exact physical slot ABI; no generated callback is kept.
 * Each family has its own provider and real typed XRT queue, never a channel.
 * Pop outputs are fresh, initialized physical slots, not language byref args.
 * ERROR is independent of allocating a diagnostic; normal flow is the exact
 * OK/EMPTY/FULL/CLOSED result. No scheduler wait or channel is introduced;
 * XRT's sequence-ring commit retains its documented progress contract.
 * SPSC admits at most one active producer and one active consumer; MPSC one
 * active consumer. Sequential role handoff is release/acquire ordered, not
 * tied to a reusable OS thread id. Close/reset require quiescent operations.
 * Only resident schema factories/metadata are allowed, never module callbacks.
 * Batch Count is the owned successful prefix, even on ERROR; no rollback is
 * invented after publication. Output slots must all be fresh and initialized. */
/* An output always remains initialized and owned by the caller, including
 * ERROR from a subsequent last-owner cleanup. The caller drops it on every
 * path; a moved prefix is never silently leaked or claimed to be rolled back. */
#define XL6_DECLARE_CONCURRENT_QUEUE(Name) \
    XL6_TEXT_API xvalue* xl6##Name##CreateTakeV1(uint64_t, xvalue*, int64_t); \
    XL6_TEXT_API xqueueresult xl6##Name##PushV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, const void*); \
    XL6_TEXT_API xqueueresult xl6##Name##PopV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, void*); \
    XL6_TEXT_API xqueuebatchresult xl6##Name##PushBatchV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, const void*, size_t); \
    XL6_TEXT_API xqueuebatchresult xl6##Name##PopBatchV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, void*, size_t); \
    XL6_TEXT_API int64_t xl6##Name##QueryV1(const xvalue*, uint64_t, unsigned int); \
    XL6_TEXT_API bool xl6##Name##CloseV1(const xvalue*, uint64_t); \
    XL6_TEXT_API bool xl6##Name##ResetV1(const xvalue*, uint64_t); \
    XL6_TEXT_API const xrtownershipadapterv1* xl6##Name##OwnershipAdapterV1( \
        xrtownershipref, const XL6_SequenceAdmissionOpsV2* const*, size_t)
XL6_DECLARE_CONCURRENT_QUEUE(SpscQueue);
XL6_DECLARE_CONCURRENT_QUEUE(MpscQueue);
XL6_DECLARE_CONCURRENT_QUEUE(MpmcQueue);
#undef XL6_DECLARE_CONCURRENT_QUEUE
enum { XL6_QUEUE_QUERY_COUNT_V1=1, XL6_QUEUE_QUERY_CAPACITY_V1,
       XL6_QUEUE_QUERY_CLOSED_V1, XL6_QUEUE_QUERY_DRAINED_V1 };
#endif
