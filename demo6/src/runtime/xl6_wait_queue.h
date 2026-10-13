#ifndef XL6_WAIT_QUEUE_H
#define XL6_WAIT_QUEUE_H
#include "xl6_concurrent_queue.h"
#include "xl6_wait_queue_abi.h"
/* Independent typed MPMC family, not Channel or a generated callback owner.
 * All nonblocking publications and waiter predicate tests share one mutex.
 * Sleep releases that mutex and never retains an ownership world mutation.
 * Close broadcasts and allows draining; capacity must be strictly positive.
 * Result/status destinations are private initialized acquisition slots, never
 * source byref. Failure leaves status untouched, including after a moved value;
 * callers must always release a moved destination on checked failure. */
XL6_TEXT_API bool xl6WaitQueueAdmitsSchemaV1(const xvalue*,uint64_t,
    const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
XL6_TEXT_API const xrtownershipadapterv1* xl6WaitQueueOwnershipAdapterV1(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
