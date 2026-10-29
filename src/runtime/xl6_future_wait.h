#ifndef XL6_FUTURE_WAIT_H
#define XL6_FUTURE_WAIT_H
#include <stdbool.h>
struct xrtownershipref;
struct xfuture;
struct xpromise;

/* A semantic wait, not a capture/ownership edge. The output and source are
 * borrowed views of real, independently owned transport slots. Describe is
 * valid ONLY inside the caller's whole-graph freeze: the exact producer and
 * all execution/registration owners must hold the same unreachable claim.
 * Ready, running, publishing, retiring, opaque and unclaimed producers have
 * no certified wait. No descriptor grants cancellation authority by itself.
 *
 * V1 describes one required input. Multi-input producers must have a separate
 * explicit completion rule; an arbitrary child or an all/race member must
 * never be substituted for this single input. Native execution is not a wait
 * merely because the frame captures a Future. */
typedef struct XL6_FutureWaitV1 {
    struct xpromise* output;
    struct xfuture* source;
} XL6_FutureWaitV1;

/* Internal resident providers. They validate identity before reading Data;
 * these functions are not generated-code callbacks or a public registration
 * surface. Each producer family owns its private state/layout validation. */
bool xl6ContinuationFutureWaitV1(struct xrtownershipref, const void*, XL6_FutureWaitV1*);
bool xl6CallbackFutureWaitV1(struct xrtownershipref, const void*, XL6_FutureWaitV1*);

typedef enum XL6_FutureWaitProof {
    XL6_FUTURE_WAIT_RESOURCE_FAILURE = -1,
    XL6_FUTURE_WAIT_UNPROVEN = 0,
    XL6_FUTURE_WAIT_CYCLE_MEMBER = 1
} XL6_FutureWaitProof;
/* One shared AND/OR proof over the single-input providers above and native
 * ALL/ANY descriptors and closed TaskGroup ALL-terminal dependencies. Ordered
 * duplicate inputs remain separate edges. The group streams its actual active
 * items into private scratch, without a copied source array or new references. An
 * unknown/ready input may progress: ALL is blocked by any blocked required
 * input, whereas ANY is blocked only when every choice is blocked. After the
 * greatest fixed point, membership requires a path back to this producer
 * inside the blocked graph. Feeders alone never authorize interruption.
 *
 * Caller holds the whole physical graph freeze and one unreachable claim on
 * every independently admitted owner. The graph is borrowed, never cancelled
 * or retained here. Small graphs use local scratch; larger ones may allocate.
 * Resource failure proves nothing and leaves graph state and the caller's
 * diagnostic untouched. A preparation may defer it without latching a user
 * callback failure. The caller still owns its cooperative interruption
 * protocol and pins the real execution frame before leaving the freeze. */
XL6_FutureWaitProof xl6FutureWaitCycleMemberV2(struct xrtownershipref, const void*);
#endif
