#ifndef XL6_OWNERSHIP_PLAN_H
#define XL6_OWNERSHIP_PLAN_H
#include "xl6_code_lease.h"
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;

typedef struct XL6_OwnershipPlan XL6_OwnershipPlan;
/* One physical lifecycle protocol shared with XRT, not a second incompatible
 * definition of Value/node retirement. The plan itself is the claim token. */
typedef struct xrtownershipadapterv1 XL6_OwnershipAdapterV1;
typedef const XL6_OwnershipAdapterV1* (*XL6_OwnershipResolve)(struct xrtownershipref, void*);
/* Resolver supplies both independently validated immutable identities BEFORE
 * Count/Trace. NULL preparation explicitly means no semantic preparation;
 * it is not a fallback for an unrecognized preparation descriptor. */
typedef bool (*XL6_OwnershipResolveV2)(struct xrtownershipref, void*,
    const XL6_OwnershipAdapterV1**, const struct xrtownershippreparationv1**);
typedef enum XL6_OwnershipPlanResult {
    XL6_OWNERSHIP_PLAN_ERROR = -1,
    XL6_OWNERSHIP_PLAN_BUSY = 0,
    XL6_OWNERSHIP_PLAN_READY = 1,
    XL6_OWNERSHIP_PLAN_FINALIZERS = 2,
    XL6_OWNERSHIP_PLAN_ROOTED = 3,
    XL6_OWNERSHIP_PLAN_PREPARATIONS = 4
} XL6_OwnershipPlanResult;

/* The root must be a REAL owning node, including its enclosing host handle or
 * module-global slots. No synthetic graph roots or estimated RC subtraction.
 * Resolver/data/code and host-specific lifecycle exclusion outlive the plan.
 * Caller serializes ALL access to the same plan, across native threads too.
 * Finalize/Finish/Destroy must not inherit an outer freeze or mutation: their
 * callbacks may wait or reenter the host. The running guard rejects callback
 * reentry, but does not replace the caller's cross-thread lifecycle mutex.
 * Each Try operation obtains a nonblocking freeze; never call it while holding
 * a lock that another active mutation needs. BUSY does not change output.
 * Every node is admitted before its Trace/Count is invoked. Unknown means
 * refusal, not a leaf, root guess, or a reason to omit a transitive edge. */
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCreate(
    struct xrtownershipref, XL6_OwnershipResolve, void*, XL6_OwnershipPlan**);
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV2(
    struct xrtownershipref, XL6_OwnershipResolveV2, void*, XL6_OwnershipPlan**);
/* Rebuild before each preparation, outside freeze for its actual invocation.
 * BUSY participants may be skipped only under their no-side-effect contract.
 * A turn is bounded by the initial participant count; new work or a busy
 * dependency may require retry, preserving the actual plan and completed work.
 * No object finalizer is allowed while a preparation remains unready. */
typedef enum XL6_OwnershipStepResult {
    /* Snapshot/admission failure is NOT a semantic callback failure. The host
     * may abort/restore and retry without permanently failing its module. */
    XL6_OWNERSHIP_STEP_ERROR = -2,
    XL6_OWNERSHIP_STEP_FAILED = -1,
    XL6_OWNERSHIP_STEP_BUSY = 0,
    XL6_OWNERSHIP_STEP_READY = 1
} XL6_OwnershipStepResult;
/* Optional physical completion protocol. BUSY retains every plan pin; FAILED
 * is an explicit callback failure even without a diagnostic. A failed duty
 * may be retried, but a READY duty is never invoked again. These immutable
 * identities are admitted before Count/Trace, just like preparation. Step
 * may return only FAILED/BUSY/READY; ERROR is reserved for plan misuse. */
typedef struct XL6_OwnershipFinishV1 {
    size_t size;
    const XL6_OwnershipAdapterV1* Adapter;
    XL6_OwnershipStepResult (*Step)(const void*, const void*);
} XL6_OwnershipFinishV1;
typedef bool (*XL6_OwnershipResolveV3)(struct xrtownershipref, void*,
    const XL6_OwnershipAdapterV1**, const struct xrtownershippreparationv1**,
    const XL6_OwnershipFinishV1**);
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV3(
    struct xrtownershipref, XL6_OwnershipResolveV3, void*, XL6_OwnershipPlan**);
/* Read-only, resident discovery under the SAME freeze as each snapshot/pin
 * transaction, including revalidation before semantic duties and Commit.
 * These are borrowed physical anchors, NEVER extra internal owning slots.
 * Discovery/data outlive the plan, obey its lifecycle exclusion, and may
 * append only known live identities. Admission still precedes Count/Trace.
 * false discards the entire partial anchor list without changing the old
 * plan; preserve the discovery callback's primary diagnostic. */
typedef bool (*XL6_OwnershipDiscoverV1)(XL6_ModuleOwnershipVisit, void*, void*);
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV4(
    struct xrtownershipref, XL6_OwnershipResolveV3, XL6_OwnershipDiscoverV1,
    void*, XL6_OwnershipPlan**);
/* Native root with independent, caller-guaranteed storage lifetime: acquire
 * one actual adapter Hold before its first snapshot. The plan owns this pin,
 * records it as an internal slot and balances Drop on success or failure.
 * This does not resurrect an invalid Value or admit an unknown descriptor. */
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV5(
    struct xrtownershipref, XL6_OwnershipResolveV3, XL6_OwnershipDiscoverV1,
    void*, XL6_OwnershipPlan**);
XL6_CODE_LEASE_API XL6_OwnershipStepResult xl6OwnershipPlanPrepare(XL6_OwnershipPlan*);
XL6_CODE_LEASE_API XL6_OwnershipStepResult xl6OwnershipPlanFinalizeV2(XL6_OwnershipPlan*);
/* Rebuild from EVERY real plan-held node, subtracting exactly those holds as
 * internal slots. New edges/nodes are discovered, pinned, and quarantined;
 * all fallible preparation precedes publication of the replacement snapshot. */
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryRefresh(XL6_OwnershipPlan*);
XL6_CODE_LEASE_API bool xl6OwnershipPlanFinalize(XL6_OwnershipPlan*);
/* Refresh + root/finalizer decision + field detach under ONE freeze, not two
 * counts with an admission gap. ROOTED preserves all fields for abort. New
 * preparations/finalizers produce their distinct statuses. V2 Finalize takes
 * a fresh graph before EACH semantic duty; a turn is bounded by the initial
 * participant count. New preparation/resurrection stops that finalize pass. */
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryCommit(XL6_OwnershipPlan*);
/* Visits each remaining duty at most once per turn. BUSY does not prevent
 * independent participants from making progress; no pins are dropped until
 * all participants report READY. Legacy bool Finish false maps to FAILED. */
XL6_CODE_LEASE_API XL6_OwnershipStepResult xl6OwnershipPlanFinishV2(XL6_OwnershipPlan*);
XL6_CODE_LEASE_API bool xl6OwnershipPlanFinish(XL6_OwnershipPlan*);
/* Abort restores quarantine before dropping holds; a committed plan first
 * requires successful Finish. Release callbacks run after leaving freeze.
 * BUSY leaves the plan alive for a later retry. Success consumes it. */
XL6_CODE_LEASE_API XL6_OwnershipPlanResult xl6OwnershipPlanTryDestroy(XL6_OwnershipPlan*);
#endif
