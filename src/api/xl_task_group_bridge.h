#ifndef XL_TASK_GROUP_BRIDGE_H
#define XL_TASK_GROUP_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "include/xl.h"
#include "xl_future_bridge.h"



typedef struct xl6_task_group xl6_task_group;
typedef struct xcancel xcancel;

/* Versioned native foundation, independent of language object layouts.
 * Config is borrowed only during Create/Child; Cancel acquires its own native
 * reference. Limit zero is unbounded. Unknown policy bits are rejected.
 * Policy bit values are the existing XRT CANCEL_ON_FAILED/CANCELLED/CLOSED. */
typedef struct XL6_TaskGroupConfigV1 {
    size_t size;
    xcancel* cancel;
    uint64_t limit;
    uint32_t cancelOn;
} XL6_TaskGroupConfigV1;
typedef struct XL6_TaskGroupStatsV1 {
    uint64_t active,added,completed,succeeded,failed,cancelled,closed,rejected;
    uint64_t firstIndex;
    int32_t firstState;
    bool hasFirst,accepting,cancelling;
} XL6_TaskGroupStatsV1;
/* Start reserves native group admission BEFORE calling the synchronous
 * starter. Rejection never calls it. Data/code is borrowed for this call only;
 * the caller independently protects entering code. The returned Future owns
 * one reference and is already tracked by the group; no add-after-start gap,
 * extra mapper, thread or coroutine is introduced. */
typedef xfuture* (*XL6_TaskGroupStartProcV1)(void*);
XL_API xl6_task_group* xl6TaskGroupCreateConfiguredV1(const XL6_TaskGroupConfigV1*);
XL_API xl6_task_group* xl6TaskGroupChildConfiguredV1(xl6_task_group*,const XL6_TaskGroupConfigV1*);
XL_API xfuture* xl6TaskGroupStartV1(xl6_task_group*,XL6_TaskGroupStartProcV1,void*);
/* Snapshot is POD and published only after successful native read. The first
 * index's absence is explicit, never a narrowed SIZE_MAX or signed cast.
 * Error/token outputs are private aligned empty owner slots, unchanged on
 * failure. Error may normally be NULL; a token is always a new owned ref. */
XL_API bool xl6TaskGroupGetV1(xl6_task_group*,XL6_TaskGroupStatsV1*);
XL_API bool xl6TaskGroupErrorCopyV1(xl6_task_group*,xerror**);
XL_API bool xl6TaskGroupCancellationV1(xl6_task_group*,xcancel**);
XL_API bool xl6TaskGroupReapV1(xl6_task_group*,uint64_t*);
/* Monotonic millisecond deadline, unlike legacy signed-millisecond Until.
 * Cancellation interrupts this waiter, not the group or its other waiters. */
XL_API int32_t xl6TaskGroupWaitV1(xl6_task_group*,uint64_t deadline,xcancel*);

struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipfinishv1;
struct xrtownershippreparationv1;
struct xrtownershipquiescencev1;
struct xcancelwatchownershipv1;
/* Quiescent frozen physical view. Every transitive owner is independently
 * admitted; these queries do not switch the global language collector. */
XL_API struct xrtownershipref xl6TaskGroupOwnershipV1(const xl6_task_group*);
XL_API const struct xrtownershipquiescencev1* xl6TaskGroupQuiescenceV1Get(void);
XL_API const struct xrtownershipadapterv1* xl6TaskGroupOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershipfinishv1**);
XL_API const struct xcancelwatchownershipv1* xl6TaskGroupParentPolicyV1Get(void);
XL_API const struct xrtownershipadapterv1* xl6TaskGroupOwnershipAdapterV2(
    struct xrtownershipref,const struct xrtownershippreparationv1**,const struct xrtownershipfinishv1**);



XL_API xl6_task_group* xl6TaskGroupCreate(void);
XL_API xl6_task_group* xl6TaskGroupChild(xl6_task_group* pParent);
XL_API void xl6TaskGroupDestroy(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupClose(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupCancel(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupBindParent(
	xl6_task_group* pGroup,
	xfuture* pParent
);
XL_API bool xl6TaskGroupAdd(
	xl6_task_group* pGroup,
	xfuture* pFuture
);
XL_API int xl6TaskGroupCount(xl6_task_group* pGroup);
XL_API int xl6TaskGroupReap(xl6_task_group* pGroup);
XL_API xfuture* xl6TaskGroupFuture(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupJoin(xl6_task_group* pGroup);
XL_API bool xl6TaskGroupJoinFor(
	xl6_task_group* pGroup,
	long long iTimeoutMs
);
XL_API bool xl6TaskGroupJoinUntil(
	xl6_task_group* pGroup,
	long long iDeadlineMs
);

#endif
