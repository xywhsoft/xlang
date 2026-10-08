#ifndef XL6_CONCURRENT_QUEUE_H
#define XL6_CONCURRENT_QUEUE_H
#include "src/runtime/xl6_sequence.h"
#include "xl6_concurrent_queue_abi.h"

/* Items is a new owned ARRAY, never a supplied/mutated output container.
 * Before the first pop, the wrapper, admission, projection and initialized
 * transfer destinations have all been prepared. On a later ERROR, Count and
 * Items still own the moved prefix; callers drop Items even when raising.
 * Maximum is an upper bound, capped by the fixed queue capacity, not a promise
 * to consume values produced later during this call. No defaults are invented
 * for nonnullable references: unpublished transfer slots are not language
 * values and only successfully moved, admitted items become visible. */
/* PopArray's identity/projection are a verified closed IR recipe, not a type
 * spelling or a user callback. Projection must be immutable resident metadata
 * with no Context; NULL selects builtin scalar conversion. For reference
 * slots only NULL is accepted and the shared retain codec is selected inside
 * the resident provider, never XRT's dynamic COW value clone. */

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
 * A normal nonempty batch returns OK for its committed prefix; EMPTY/FULL/
 * CLOSED describe a batch that moved nothing. A zero-size request is OK.
 * Batch Count is the owned successful prefix, even on ERROR; no rollback is
 * invented after publication. Output slots must all be fresh and initialized. */
/* An output always remains initialized and owned by the caller, including
 * ERROR from a subsequent last-owner cleanup. The caller drops it on every
 * path; a moved prefix is never silently leaked or claimed to be rolled back. */
/* Immutable-domain probes require the genuine family, canonical seed identity,
 * exact physical tuple and complete owned admission policy. They do not enter
 * a producer/consumer lane or read mutable queue slots; BUSY is permitted and
 * an existing diagnostic is preserved. Native policy equivalence is metadata
 * only, not a user callback or a scan of element storage. */
#define XL6_DECLARE_CONCURRENT_QUEUE(Name) \
    XL6_TEXT_API bool xl6##Name##AdmitsSchemaV1(const xvalue*, uint64_t, const XL6_SequenceLayoutV1*, const XL6_SequenceAdmissionV3*); \
    XL6_TEXT_API xvalue* xl6##Name##CreateTakeV1(uint64_t, xvalue*, int64_t); \
    XL6_TEXT_API xqueueresult xl6##Name##PushV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, const void*); \
    XL6_TEXT_API xqueueresult xl6##Name##PopV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, void*); \
    XL6_TEXT_API xqueuebatchresult xl6##Name##PushBatchV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, const void*, size_t); \
    XL6_TEXT_API xqueuebatchresult xl6##Name##PopBatchV1(const xvalue*, uint64_t, uint64_t, size_t, const xrttype*, void*, size_t); \
    XL6_TEXT_API xqueuebatchresult xl6##Name##PushArrayV2(const xvalue*, uint64_t, const xvalue*, uint64_t); \
    XL6_TEXT_API XL6_QueueArrayResultV2 xl6##Name##PopArrayV2(const xvalue*, uint64_t, uint64_t, int64_t, const xvalueconverter*); \
    XL6_TEXT_API int64_t xl6##Name##QueryV1(const xvalue*, uint64_t, unsigned int); \
    XL6_TEXT_API bool xl6##Name##CloseV1(const xvalue*, uint64_t); \
    XL6_TEXT_API bool xl6##Name##ResetV1(const xvalue*, uint64_t); \
    XL6_TEXT_API const xrtownershipadapterv1* xl6##Name##OwnershipAdapterV1( \
        xrtownershipref, const XL6_SequenceAdmissionOpsV2* const*, size_t)
XL6_DECLARE_CONCURRENT_QUEUE(SpscQueue);
XL6_DECLARE_CONCURRENT_QUEUE(MpscQueue);
XL6_DECLARE_CONCURRENT_QUEUE(MpmcQueue);
#undef XL6_DECLARE_CONCURRENT_QUEUE
#endif
