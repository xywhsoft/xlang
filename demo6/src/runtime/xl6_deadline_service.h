#ifndef XL6_DEADLINE_SERVICE_H
#define XL6_DEADLINE_SERVICE_H
#include "include/xruntime_decl.h"
#include "xl6_deadline_module_abi.h"
typedef struct XL6_DeadlineTicketV1 XL6_DeadlineTicketV1;
typedef enum XL6_DeadlineResultV1 {
    XL6_DEADLINE_EXPIRED_V1=0,
    XL6_DEADLINE_FAILED_V1
} XL6_DeadlineResultV1;
/* Private resident native transport, NOT a language callback certificate.
 * Tables/code outlive all bindings and service inspection pins. Notify and
 * Drop are mechanical native operations: no language CFG/semantic destructor,
 * blocking wait, allocation or module-local callback. Ownership is a pure
 * exact view of the ONE real Data owner transferred by BindTake. */
typedef struct XL6_DeadlineTargetV1 {
    size_t size;
    void (*Notify)(const void*,XL6_DeadlineResultV1,const xerror*);
    void (*Drop)(const void*);
    xrtownershipref (*Ownership)(const void*);
} XL6_DeadlineTargetV1;
/* Zero-initialize; stable, noncopyable, caller-serialized Bind/reuse. The Data
 * reference must keep this storage alive until Drop (or caller independently
 * owns it). After successful Bind it may fire before Bind returns. Detach can
 * be reentrant from Notify and is idempotent; once dispatch claimed the ticket,
 * Notify may still run and physical ownership ends only at its tail. A fired
 * ticket remains owned until Detach; callback owners must eventually detach.
 * IsIdle means the service will no longer touch ticket; the preceding mechanical
 * Data.Drop may still finish. Reuse requires independently owned storage/Data,
 * never a borrowed pointer into a target whose last owner may be that Drop.
 * Owner/once are the sole lock-free public fields;
 * all remaining private fields require service gate or whole-graph Freeze. */
struct XL6_DeadlineTicketV1 {
    xatomicptr owner;
    xatomic32 once;
    XL6_DeadlineTicketV1 *previous,*next;
    size_t heapIndex;
    xdeadline due;
    const XL6_DeadlineTargetV1* policy;
    const void* data;
    bool bound,dispatching,fired;
};
/* Takes *data on success ONLY. Failure leaves ticket idle and input unchanged.
 * Capacity counts logical accepted bindings, not independent physical tails.
 * All allocation/admission precedes dispatch; NEVER schedules via Net engine. */
XL6_CODE_LEASE_API bool xl6DeadlineBindTakeV1(XL6_DeadlineServiceV1*,XL6_DeadlineTicketV1*,
    xdeadline,const XL6_DeadlineTargetV1*,const void**);
XL6_CODE_LEASE_API void xl6DeadlineDetachV1(XL6_DeadlineTicketV1*);
XL6_CODE_LEASE_API bool xl6DeadlineTicketIsIdleV1(const XL6_DeadlineTicketV1*);
/* Borrowed frozen owning edge: one service lease while ticket is not idle. */
XL6_CODE_LEASE_API xrtownershipref xl6DeadlineTicketServiceOwnershipV1(const XL6_DeadlineTicketV1*);
/* Close refuses NEW work but preserves accepted deadlines. Destroy is a
 * non-destructive busy check; Completed may wait only for already-detached
 * callback/thread tails, never for a future deadline/owner Detach. Both require
 * serialized terminal owners; failure does not consume the creator. A claimed
 * source refuses ordinary destruction until that plan's Clear. Native
 * callbacks cannot destroy their own worker. Pins preserve retired allocation. */
/* False is authoritative even if diagnostic allocation failed. */
XL6_CODE_LEASE_API bool xl6DeadlineServiceCheckV1(XL6_DeadlineServiceV1*);
XL6_CODE_LEASE_API bool xl6DeadlineFromMillisecondsV1(int64_t,xdeadline*);
XL6_CODE_LEASE_API const xrtownershipadapterv1* xl6DeadlineServiceOwnershipAdapterV1(
    xrtownershipref,const xrtownershippreparationv1**);
XL6_CODE_LEASE_API const xrtownershipquiescencev1* xl6DeadlineServiceQuiescenceV1Get(void);
#endif
