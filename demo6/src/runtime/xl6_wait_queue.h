#ifndef XL6_WAIT_QUEUE_H
#define XL6_WAIT_QUEUE_H
#include "xl6_concurrent_queue.h"
#include "xl6_channel_abi.h"
/* Independent typed MPMC family, not Channel or a generated callback owner.
 * All nonblocking publications and waiter predicate tests share one mutex.
 * Sleep releases that mutex and never retains an ownership world mutation.
 * Close broadcasts and allows draining; capacity must be strictly positive.
 * Result/status destinations are private initialized acquisition slots, never
 * source byref. Failure leaves status untouched, including after a moved value;
 * callers must always release a moved destination on checked failure. */
XL6_TEXT_API xvalue* xl6WaitQueueCreateTakeV1(uint64_t,xvalue*,int64_t);
XL6_TEXT_API bool xl6WaitQueueAdmitsSchemaV1(const xvalue*,uint64_t,
    const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
XL6_TEXT_API xqueueresult xl6WaitQueuePushV1(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*);
XL6_TEXT_API bool xl6WaitQueueReceiveV1(const xvalue*,uint64_t,uint64_t,const xrttype*,
    XL6_ChannelSlotV1,XL6_ChannelWaitV1,int64_t,XL6_ChannelStatusV1*);
XL6_TEXT_API int64_t xl6WaitQueueQueryV1(const xvalue*,uint64_t,unsigned int);
XL6_TEXT_API bool xl6WaitQueueCloseV1(const xvalue*,uint64_t);
XL6_TEXT_API const xrtownershipadapterv1* xl6WaitQueueOwnershipAdapterV1(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
