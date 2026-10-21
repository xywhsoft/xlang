#ifndef XL6_FUTURE_CALLBACK_INTERRUPT_H
#define XL6_FUTURE_CALLBACK_INTERRUPT_H
#include "xl6_future_callback.h"
#include "xl6_deadline_service.h"
/* Private resident, exclusive-operation protocol. Use V5 Allocate/Cleanup and
 * V1 Discard. Arm BEFORE constructing native work. False leaves the private
 * initialized frame for Discard; do not retry. No callback/capture cleanup runs
 * on a timer/cancellation thread. The optional transport is uniquely contained
 * in this SAME frame, not a promise shell or an independent graph node.
 *
 * Stopped is a pre-submit optimization, not a race barrier. Start takes an
 * actual source after submission, or NULL ONLY after a pre-start stop. On
 * success the queue consumes the builder. On failure Discard still owns it;
 * the caller cancels/drains any native work it already accepted.
 *
 * Selection is at queued callback entry: a stop before selection supersedes
 * an undelivered native result. After selection it cannot revoke delivery.
 * Timeout is FAILED/TIMEOUT, parent cancellation is CANCELLED. A source is
 * retained until its REAL terminal event; cancellation is only a request.
 * Failure of the deadline service is FAILED with its first diagnostic.
 * V1-6 are unchanged and do not select this optional component. */
XL6_CODE_LEASE_API bool xl6FutureCallbackInterruptArmV1(void*,xcancel*,XL6_DeadlineServiceV1*,xdeadline);
XL6_CODE_LEASE_API bool xl6FutureCallbackInterruptStoppedV1(const void*);
XL6_CODE_LEASE_API xfuture* xl6FutureCallbackInterruptStartV1(struct XL6_ContinuationExecutor*,void*,xfuture*);
XL6_CODE_LEASE_API const xcancelwatchownershipv1* xl6FutureCallbackInterruptCancelPolicyV1Get(void);
#endif
