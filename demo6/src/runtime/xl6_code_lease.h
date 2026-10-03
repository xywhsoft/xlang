#ifndef XL6_CODE_LEASE_H
#define XL6_CODE_LEASE_H
#include <stdbool.h>
#include <stdint.h>
#include "xl6_code_owner.h"
#ifndef XL6_CODE_LEASE_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CODE_LEASE_API __declspec(dllexport)
#else
#define XL6_CODE_LEASE_API
#endif
#endif

/* Shared residency capability, NOT a new execution/root admission. One host
 * acquire backs all retained children. Retain is valid after host Close while
 * the caller owns a reference; it cannot resurrect a released capability.
 * This mechanism does not decide when module globals may be retired, nor does
 * it collect cycles formed by module-owned values holding their own module.
 * Resident acquisition/RC/transfer/destruction operations participate in the
 * XRT mutation domain, before host or scope-list locks and through rollback.
 * Last Release detaches the real host acquisition, ends its RC mutation, then
 * invokes the host release. Scope Leave likewise ends its list transition
 * before releasing the final lease. An enclosing caller mutation is not
 * implicitly ended: that caller must keep arbitrary cleanup outside it.
 * This does not certify a custom owner or arbitrary traced child as frozen. */
typedef struct XL6_CodeLease XL6_CodeLease;
typedef struct XL6_CodeDrop XL6_CodeDrop;
typedef void (*XL6_CodeDropProc)(void* value, void* context);
typedef enum XL6_CodeLeaseAdmission {
    XL6_CODE_LEASE_ACQUIRED = 0,
    XL6_CODE_LEASE_CLOSED,
    XL6_CODE_LEASE_RESOURCE_FAILURE,
    XL6_CODE_LEASE_INVALID
} XL6_CodeLeaseAdmission;

/* Caller holds an entry lease throughout acquisition and failure rollback.
 * NULL owner explicitly means externally resident code; success returns NULL.
 * Output is cleared on failure, and prior diagnostics remain primary. */
XL6_CODE_LEASE_API XL6_CodeLeaseAdmission xl6CodeLeaseAcquire(const XL6_CodeOwner*, XL6_CodeLease**);
/* Callback execution is an actual external root. The scope owns one retained
 * reference until its resident Leave, after generated code has returned.
 * Enter/Leave allocate nothing, including on a previously unknown native
 * thread. Storage is caller-owned, zero initialized, same-thread and LIFO.
 * Fields are private to the resident implementation; do not copy a live scope.
 * NULL parent is the externally-resident case and needs no registration.
 * Only Enter/Leave transitions are mutations. The intervening invocation is
 * NOT a long-lived mutation scope: it may wait/park while its real lease ref
 * remains an external activity root. Generated writes need their own boundary. */
typedef struct XL6_CodeScope {
    XL6_CodeLease* lease;
    struct XL6_CodeScope* previous;
    struct XL6_CodeScope* next;
    unsigned long long threadId;
} XL6_CodeScope;
XL6_CODE_LEASE_API void xl6CodeScopeEnter(XL6_CodeScope*, XL6_CodeLease*);
XL6_CODE_LEASE_API void xl6CodeScopeLeave(XL6_CodeScope*);
/* Read only under whole-domain Freeze (or equivalent external quiescence).
 * A live generated invocation must be excluded BEFORE its module globals are
 * traced, not discovered as a root after reading concurrently written fields. */
XL6_CODE_LEASE_API bool xl6CodeOwnerScopeActive(const XL6_CodeOwner*);
/* Reuse an accepted capability only from this thread's active callback stack
 * and the exact same copied host identity. This remains valid after Close.
 * Otherwise perform ordinary (fallible) admission. This does not reopen a
 * closed owner, discover a capability in another thread, or collect cycles. */
XL6_CODE_LEASE_API XL6_CodeLeaseAdmission xl6CodeLeaseAcquireScoped(const XL6_CodeOwner*, XL6_CodeLease**);
/* Allocation-free. Reference overflow is a contract violation, not a reason
 * to silently skip a destructor. Each concurrent caller already owns a ref. */
XL6_CODE_LEASE_API XL6_CodeLease* xl6CodeLeaseRetain(XL6_CodeLease*);
/* Last release may unmap generated code. Only resident code, or a caller with
 * another independent entry/residency lease, may perform that release. */
XL6_CODE_LEASE_API void xl6CodeLeaseRelease(XL6_CodeLease*);
/* One node for this shared lease, with its actual retained-child count and
 * exactly one edge to the host owner acquired at creation. A legacy/unknown
 * owner refuses inspection; it is never silently treated as a leaf. NULL is
 * an empty view. Caller holds storage/code residency and whole-graph quiescence. */
struct xrtownershipref;
XL6_CODE_LEASE_API struct xrtownershipref xl6CodeLeaseOwnership(const XL6_CodeLease*);
struct xrtownershipadapterv1;
/* The resident lease is known; its arbitrary host owner still needs separate
 * pre-trace admission. Actual host acquisitions survive Clear until unpin. */
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6CodeLeaseOwnershipAdapterV1(struct xrtownershipref);
/* Borrowed exact immutable owner identity, only under whole-domain freeze or
 * equivalent quiescence. Does not invoke host hooks, inspect owner storage,
 * retain, authorize an unknown policy, or reopen admission after Close. */
XL6_CODE_LEASE_API bool xl6CodeLeaseOwnerMatches(const XL6_CodeLease*, const XL6_CodeOwner*);

/* Transactional Future/destructor adapter. Create borrows parent and does not
 * consume value/context. Its failure never runs drop. It copies the callback
 * and keeps one child lease; caller aborts an unaccepted record or transfers it
 * exactly once to a callback owner. The original parent/entry lease must stay
 * alive through abort on a generated caller stack. */
XL6_CODE_LEASE_API XL6_CodeDrop* xl6CodeDropCreate(XL6_CodeLease* parent, XL6_CodeDropProc drop, void* context);
XL6_CODE_LEASE_API void xl6CodeDropAbort(XL6_CodeDrop*);
/* XRT Future free-proc ABI. The record is detached before drop runs. No record,
 * generated descriptor or generated callback is accessed after code release.
 * Drop's own diagnostic remains visible; host release diagnostics are isolated. */
XL6_CODE_LEASE_API void xl6CodeDropInvoke(void* value, void* record);

/* Object finalization has two distinct tails: Finalize borrows the still-live
 * receiver/fields; context Release runs after ALL backing fields have ended.
 * Keep code resident across both, including a cursor created by Finalize.
 * Bind allocates one private record and transfers context only on success.
 * A non-NULL context requires both Trace and Release; Finalize may not free it.
 * Parent is borrowed and retained, including after Close. NULL parent means
 * externally resident code. Caller holds an independent entry/parent lease
 * throughout binding and failure rollback. This does not collect module cycles
 * or select automatic object ownership on behalf of the IR. */
struct xvalue;
typedef void (*XL6_CodeFinalizeProc)(struct xvalue*, void*);
typedef bool (*XL6_CodeContextTrace)(const void*, XL6_ModuleOwnershipVisit, void*);
typedef void (*XL6_CodeContextRelease)(void*);
XL6_CODE_LEASE_API bool xl6CodeFinalizerBind(struct xvalue*, XL6_CodeLease*,
    XL6_CodeFinalizeProc, void*, XL6_CodeContextTrace, XL6_CodeContextRelease);

/* Verified class storage ABI. Create allocates a LIFO object and transfers a
 * copyable code capability BEFORE any field/create callback can publish it.
 * COW/deep copies retain that capability; only actual user destructors prepare
 * a unique finalization duty. Ordinary classes keep their value-copy semantics.
 * NULL finalize means no user destructor, not externally resident code.
 * CommitTake commits construction (and its optional duty) without allocation, consuming
 * one input owner on both outcomes. Failure preserves the primary diagnostic.
 * Abandoned pending objects release initialized fields/context, not user code.
 * Caller holds an entry/scope through creation and rollback. No module-cycle
 * collection or graph safepoint is implied. Returned function addresses are
 * resident (never generated PE import thunks). */
typedef struct XL6_ClassRuntimeV1 {
    struct xvalue* (*Create)(const XL6_CodeOwner*, XL6_CodeFinalizeProc);
    struct xvalue* (*CommitTake)(struct xvalue*);
} XL6_ClassRuntimeV1;
XL6_CODE_LEASE_API const XL6_ClassRuntimeV1* xl6ClassRuntimeV1Get(void);
typedef struct XL6_ClassRuntimeV2 {
    struct xvalue* (*Create)(const XL6_CodeOwner*, bool (*)(struct xvalue*, void*));
    struct xvalue* (*CommitTake)(struct xvalue*);
    bool (*BindIdentity)(struct xvalue*, uint64_t (*)(const struct xvalue*, void*),
        bool (*)(const struct xvalue*, const struct xvalue*, void*));
} XL6_ClassRuntimeV2;
/* V2 explicitly covers immutable generated identity and a BOOL finalizer
 * outcome, including failure without an allocated error. V1 remains a
 * separate, unchanged void-callback ABI; never cast between the tables. */
XL6_CODE_LEASE_API const XL6_ClassRuntimeV2* xl6ClassRuntimeV2Get(void);
/* Only the resident Class factory's exact lifetime/finalizer family, never
 * arbitrary legacy CodeFinalizerBind contexts. Requires complete graph admission. */
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ClassOwnershipAdapterV1(struct xrtownershipref);
/* Read-only discovery for the exact resident Class factory family and exact
 * copied code owner. Includes actual COW/deep backings, not creation shells.
 * Caller holds freeze through admission/snapshot/pins; anchors own nothing.
 * Legacy finalizer bindings and arbitrary contexts remain outside the family. */
XL6_CODE_LEASE_API bool xl6ClassOwnershipDiscoverV1(const XL6_CodeOwner*,
    XL6_ModuleOwnershipVisit, void*);
#endif
