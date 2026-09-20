#ifndef XL6_CONTINUATION_QUEUE_H
#define XL6_CONTINUATION_QUEUE_H
/* Queue/ticket mutation enters the XRT domain before the queue lock, including
 * transport registration, abort and retirement detach/credit publication.
 * Waiting, run and release callbacks do not hold that admission. An active or
 * detached retiring ticket is refused by
 * its ownership view, and its resident code scope holds a real activity root.
 * Mutable frame/data adapters still require complete participation of their
 * own; this queue contract alone does not admit module-cycle collection. */
#include <stdbool.h>
#include <stddef.h>
#include "xl6_code_lease.h"
struct xrtownershipref;
union xfuturewatch;
typedef bool (*XL6_ContinuationTrace)(const void*, bool (*)(struct xrtownershipref, void*), void*);
#ifndef XL6_CONTINUATION_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_CONTINUATION_API __declspec(dllexport)
#else
#define XL6_CONTINUATION_API
#endif
#endif

/* Runtime-only transport; no AST, type lookup or code generation dependency.
 * An executor owns RunOne. Producers only signal an already-reserved ticket.
 * This queue does not create workers or choose a blocking/execution policy. */
typedef struct XL6_ContinuationQueue XL6_ContinuationQueue;
/* Private resident executor slot. WaitRunOwned acquires one REAL queue
 * reference into queue for the entire call. While parked, the owning executor
 * traces that exact field; while running it remains an uninspectable native
 * entry. The slot is zero-initialized, never copied, and uniquely used by one
 * worker. Its enclosing allocation must outlive that worker's join. Reads by
 * ownership adapters require a whole-graph freeze. */
typedef struct XL6_ContinuationWaitSlot {
    XL6_ContinuationQueue* queue;
    bool parked;
} XL6_ContinuationWaitSlot;
typedef void (*XL6_ContinuationProc)(void*);
/* Private resident storage contract. Retain/Release operate on real storage
 * references; neither performs payload cleanup while a transport owns it.
 * Ownership returns the ONE allocation identity, not a second ticket node.
 * These functions must outlive the queue. This table does not certify a
 * native producer: a collector independently resolves the returned identity. */
typedef struct XL6_ContinuationStorageV2 {
    bool (*Retain)(const void*);
    void (*Release)(const void*);
    struct xrtownershipref (*Ownership)(const void*);
    /* Optional closed transport policy and logical activation transfer.
     * These resident operations allocate nothing and invoke no semantic user
     * callbacks. Commit/Abort run within the queue's mutation/lock boundary. */
    bool (*WatchInit)(const void*, union xfuturewatch*);
    void (*Commit)(const void*);
    void (*Abort)(const void*);
    /* Optional exactly-once source/interrupt arbitration, including READY
     * registration. It signals the existing queue event, never executes CFG. */
    void (*Ready)(const void*);
} XL6_ContinuationStorageV2;
enum { XL6_CONTINUATION_COMMITTED = 1u, XL6_CONTINUATION_READY = 2u,
    XL6_CONTINUATION_TRANSPORT_RELEASED = 4u, XL6_CONTINUATION_RUNNING = 8u };
typedef struct XL6_ContinuationTicket {
    /* Private after Reserve; zero-initialize before first use. Storage stays
     * alive until release or successful Abort. Never copy an active ticket. */
    XL6_ContinuationQueue* queue;
    struct XL6_ContinuationTicket* next;
    XL6_ContinuationProc run;
    XL6_ContinuationProc release;
    void* data;
    struct XL6_ContinuationTicket* predecessor;
    struct XL6_ContinuationTicket* successor;
    unsigned flags;
    bool transportOwned;
    /* One real shared lease, transferred (not re-acquired) to a successor.
     * The successor borrows this field during handoff; it becomes the owning
     * slot only after the predecessor's generated callbacks and scope end. */
    XL6_CodeLease* codeLease;
    /* One physical frame lease belongs to the registration owner until
     * Commit, then to the queue. A registered Watch takes a second storage
     * lease, returned by TransportReleased. Neither is a host Value root. */
    size_t ownershipRefs;
    XL6_ContinuationTrace ownershipTrace;
    /* V2-only resident binding, installed before Watch registration. Reserve
     * initializes it to NULL for legacy caller-owned tickets. V2 release(data)
     * retires payload but cannot free storage: the queue returns its physical
     * reference afterward, independently of native/collector/Watch pins.
     * This private header requires a same-source rebuild, not mixed-size ABI. */
    const XL6_ContinuationStorageV2* storage;
    /* Real queue ownership list, distinct from readiness order. Removal at
     * dequeue transfers the one base reference to the resident running local. */
    struct XL6_ContinuationTicket *ownedNext, *ownedPrevious;
} XL6_ContinuationTicket;
typedef enum XL6_ContinuationAdmission {
    XL6_CONTINUATION_ADMITTED = 0,
    XL6_CONTINUATION_FULL,
    XL6_CONTINUATION_CLOSED,
    XL6_CONTINUATION_INVALID,
    XL6_CONTINUATION_RESOURCE_FAILURE
} XL6_ContinuationAdmission;

XL6_ContinuationQueue* xl6ContinuationQueueCreate(size_t capacity);
XL6_ContinuationAdmission xl6ContinuationReserve(
    XL6_ContinuationQueue*, XL6_ContinuationTicket*,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data);
/* Acquire a shared code lease for a root activation. This admission may fail
 * to allocate its lease record; no ticket/capacity is committed on failure.
 * The entering call must itself
 * hold an external entry lease until this function returns (including failed
 * admission). Successors inherit the pin; only final physical retirement or
 * an unregistered root Abort releases it, outside the queue lock. Run/release
 * execute inside a resident CodeScope, so their children can inherit accepted
 * code after Close. This does not reopen admission for a NEW root activation. */
XL6_ContinuationAdmission xl6ContinuationReserveOwned(
    XL6_ContinuationQueue*, XL6_ContinuationTicket*, XL6_ContinuationProc run,
    XL6_ContinuationProc release, void* data, const XL6_CodeOwner*);
/* Bind the exact owning slots released by release(data), before registering
 * any producer. Borrowed views and code pointers are not owning slots.
 * Failed binding leaves the private reservation untouched. */
XL6_CONTINUATION_API bool xl6ContinuationTraceBind(XL6_ContinuationTicket*, XL6_ContinuationTrace);
/* Borrowed physical view of the embedded ticket/frame allocation. Only fully
 * committed, non-running, non-handoff tickets can be inspected. The queue's
 * real execution lease remains an external root; the Watch reports its own
 * storage lease separately. Unbound data or an opaque host code owner rejects
 * inspection. Caller supplies WHOLE-graph quiescence and code residency; this
 * API does not stop workers, claim a frame, or authorize module unloading. */
XL6_CONTINUATION_API struct xrtownershipref xl6ContinuationOwnership(const XL6_ContinuationTicket*);
/* Called only by predecessor's run callback, before it returns. This is the
 * next suspension of the SAME activation, not a spawn: one successor inherits
 * its capacity even after Close. Abort before any event restores the parent.
 * All three successor events may arrive during predecessor execution, but
 * the successor cannot run until predecessor release has returned. Thus the
 * capacity bounds logical activations and at most two physical tickets per
 * activation coexist during handoff. No allocation or callback here.
 * The predecessor is private to its run callback; concurrent successor/abort
 * operations by other callers are invalid. Registration producers only signal
 * Ready/TransportReleased. A release callback cannot reserve a successor. */
XL6_CONTINUATION_API XL6_ContinuationAdmission xl6ContinuationReserveSuccessor(
    XL6_ContinuationTicket* predecessor, XL6_ContinuationTicket* successor,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data);
/* All three independent events must happen exactly once, in any order:
 * Commit: registration and frame ownership handoff are complete.
 * Ready: a terminal source result or an explicitly authorized interruption
 * is ready for the normal resume CFG; the shared source may remain pending.
 * TransportReleased: Watch no longer accesses the embedded frame storage.
 * The last event can publish the ticket to a concurrent executor; callers
 * must not touch it afterwards. These operations allocate nothing and never
 * execute run/release. Reservation guarantees queue space. */
XL6_CONTINUATION_API void xl6ContinuationCommit(XL6_ContinuationTicket*);
void xl6ContinuationReady(XL6_ContinuationTicket*);
void xl6ContinuationTransportReleased(XL6_ContinuationTicket*);
/* Registration failure before any event: return ownership to the caller,
 * without running or releasing language state on the admission thread. */
XL6_CONTINUATION_API bool xl6ContinuationAbort(XL6_ContinuationTicket*);
/* Run and release outside the queue lock and mutation admission; release
 * follows run exactly once after detaching the uninspectable ticket. A runner
 * retains the real code lease and queue credit through release and its tail,
 * even if release frees its own ticket. Frame adapters must coordinate their
 * physical Free separately; no producer may touch a retired ticket. */
bool xl6ContinuationRunOne(XL6_ContinuationQueue*);
/* Executor-only blocking dequeue. Returns false only after Close and final
 * lease retirement; accepted pending tickets keep workers alive. */
bool xl6ContinuationWaitRunOne(XL6_ContinuationQueue*);
bool xl6ContinuationWaitRunOwned(XL6_ContinuationQueue*, XL6_ContinuationWaitSlot*);
bool xl6ContinuationQueueIsIdle(XL6_ContinuationQueue*);
/* Root execution credits, including provisional reservations and a final
 * code-owner release in progress. Used by admission to provision workers
 * BEFORE a root can publish. A successor inherits, never duplicates a credit. */
size_t xl6ContinuationQueueActivationCount(XL6_ContinuationQueue*);
void xl6ContinuationQueueClose(XL6_ContinuationQueue*);
/* External lifecycle synchronization required. A final host code-owner hook
 * retains a resident queue/execution lease even after its ticket is freed.
 * Busy returns false without
 * freeing anything. Close rejects new work but does not discard reservations. */
bool xl6ContinuationQueueDestroy(XL6_ContinuationQueue*);
/* Actual queue allocation ownership. Create supplies one owner reference;
 * successful Destroy consumes one only after Close and work retirement.
 * Native/plan Hold extends this SAME allocation, including synchronization
 * objects. Inspection refuses provisional, running, handoff and native-entry
 * phases. Admission requires an exact allow-list of resident storage tables
 * before any ticket Ownership callback; every child needs its own admission.
 * The preparation duty closes new admission, not the accepted execution CFG.
 * Individual frame/Future preparations must settle before graph Clear. */
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
XL6_CONTINUATION_API struct xrtownershipref xl6ContinuationQueueOwnership(const XL6_ContinuationQueue*);
XL6_CONTINUATION_API const struct xrtownershipadapterv1* xl6ContinuationQueueOwnershipAdapterV1(
    struct xrtownershipref,const XL6_ContinuationStorageV2* const*,size_t,
    const struct xrtownershippreparationv1**);
/* Read-only under the caller's whole-graph freeze. This queue must have been
 * claimed by that exact plan and prepared; the ticket is still its real held
 * execution slot. An external queue or a mere Close is NOT authorization. */
XL6_CONTINUATION_API bool xl6ContinuationQueuePrepareOwns(const XL6_ContinuationTicket*,const void*);
/* Same frozen claim/preparation proof for the executor's own queue slot. */
bool xl6ContinuationQueuePreparedFor(const XL6_ContinuationQueue*,const void*);
#endif
