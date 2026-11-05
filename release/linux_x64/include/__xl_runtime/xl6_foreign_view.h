#ifndef XL6_FOREIGN_VIEW_H
#define XL6_FOREIGN_VIEW_H

#include "xl6_code_lease.h"
#include "xl6_ownership_catalog.h"
#include "xl6_foreign_callback_scope.h"
#include "xl6_foreign_context.h"
#include <stddef.h>
#include <stdint.h>

typedef struct XL6_ForeignView XL6_ForeignView;
struct xl6_foreign_error_result;

/* Compiler/native-publisher contract, not a sandbox for arbitrary C pointers.
 * The copied descriptor and every transitive native dependency are protected
 * by the supplied code owner. NULL code owner means externally resident C.
 * Family names are exact canonical physical C identities within one native
 * package/runtime. Aliases and native subclasses share their root family.
 *
 * Watch is a nonallocating UI-thread attachment with zeroed stable storage.
 * It invokes no user code and retains no input on failure. Notification is
 * once, before native user destruction/free, after detaching the storage.
 * Unwatch is nonallocating and safe on detached zeroed storage. Watch and
 * Unwatch are either both present or both absent. An externally destroyable
 * root also needs native CallBegin/CallEnd physical-lifetime protection.
 *
 * Destroy returns true only after consuming the native owner. Failure that
 * leaves a native owner retains its observation for checked retry; a consumed
 * owner must already have delivered invalidation, even on failure. It never unwinds
 * across C. CallEnd and Destroy can run native/language user code; no mutation
 * scope, collector Freeze or queue lock may enclose them.
 *
 * TraceRaw describes ALL actual refcounted owning slots in a root's uniquely
 * owned C payload, including its unique internal allocations. Independently
 * shared native nodes remain separate references and need their own admission.
 * Child views borrow this payload; they do not duplicate its owning edges.
 * TraceRaw is read-only under whole-domain Freeze, with no allocation, locks,
 * language invocation, reference transitions or policy guessing.
 * Busy is a creator-thread read-only probe of independent native activity,
 * valid both at idle retirement and under whole-domain Freeze. An observed root requires it: the resident active count alone
 * cannot see operations begun by an external C event loop. */
typedef struct XL6_ForeignViewDescriptorV1 {
    size_t size;
    uint64_t abiTag;
    const char* family;
    size_t watchBytes, watchAlignment;
    bool (*Watch)(void* raw,void* storage,void (*invalidate)(void*),void* user);
    void (*Unwatch)(void* storage);
    bool (*Destroy)(void* raw);
    size_t callBytes, callAlignment;
    bool (*CallBegin)(void* root,void* storage);
    bool (*CallEnd)(void* storage);
    bool (*TraceRaw)(const void*,XL6_ModuleOwnershipVisit,void*);
    bool (*Busy)(const void*);
    uint64_t flags;
    /* Cold compiler-record cleanup, including its outer allocation and every
     * partial payload dependency. Cleanup must not depend on borrowed inputs
     * whose call has ended. False retains this exact owner for retry;
     * true consumes it even when a separate checked failure was reported. */
    bool (*DestroyUnadopted)(void* raw);
    /* Optional root logical close request: never consumes its native owner
     * or invalidates physical observation. Rejection leaves the view open;
     * acceptance and checked diagnostic success are independent. Repeated
     * accepted requests coalesce. Borrowed children use native parent ownership. */
    bool (*RequestClose)(void* raw);
} XL6_ForeignViewDescriptorV1;
#define XL6_FOREIGN_VIEW_V1_TAG UINT64_C(0x584C364656494534)
/* An active close retains the native owner in the checked retirement queue.
 * The native logical destructor runs only after all active scopes return;
 * this is required when close may be temporarily refused by native nesting.
 * Ordinary Context roots retain their immediate logical-close behavior. */
#define XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY UINT64_C(1)
/* For every valid factory output transferring one actual native owner, Watch
 * establishes physical observation without allocation, including a live
 * closing output. Ordinary method eligibility is a separate checked result.
 * This is a trusted native ABI promise, not validation of arbitrary pointers. */
#define XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH UINT64_C(2)
/* The checked destructor owns its native operation scope and actual idle
 * activity/cache preflight. Graph Busy remains independent; it must not
 * prevent an idle native destructor from preparing other ready resources. */
#define XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE UINT64_C(4)
/* The root adapter separately reports every actual cleanup failure through
 * the checked channel, including a failed result with no diagnostic. Destroy
 * false only means its owner remains unconsumed and may wait without error.
 * This does not change the conservative DestroyUnadopted contract. */
#define XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE UINT64_C(8)
/* Cold opt-in to the independent semantic FinalizationV1 protocol. The
 * native checked destructor reports failure separately from owner
 * consumption and may make durable progress while retaining that owner.
 * Only a continuation-aware plan may invoke it as a graph finalizer. */
#define XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE UINT64_C(16)

/* Stable compiler-owned storage; never copy a live call or callback scope.
 * nativeStorage is a separately typed C record emitted from the declaration,
 * not a guessed/fixed-capacity byte buffer. The root's native CallEnd may
 * destroy its whole payload. After it returns, resident code releases pins. */
typedef struct XL6_ForeignViewCallV1 {
    struct XL6_ForeignViewCallV1* self;
    XL6_ForeignView* view;
    XL6_ForeignView* root;
    void* raw;
    void* nativeStorage;
    XL6_ForeignCallContextV1 execution;
} XL6_ForeignViewCallV1;

/* Failure never consumes raw. BorrowChild requires a current successful call
 * on the same creator thread/root; repeated exposure reuses its live control
 * record. Neither ordinary copying nor an existing lookup allocates a wrapper.
 * All language owning references keep the root/context alive. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootV1(const void*,const XL6_ForeignViewDescriptorV1*,const XL6_CodeOwner*);
/* A native subgraph may contain independent shared nodes, rather than only
 * uniquely owned payload slots. This optional domain admits those exact Ops.
 * It is immutable, has no Load hook, and its Resolve/AdmitsRead are independent
 * of catalog policies. The supplied code owner protects the domain, its native
 * tables and all transitive code until every plan callback has returned.
 * A validated DomainV2 can additionally answer exact semantic-finalization
 * queries without a Load hook. This capability remains available through
 * cleared roots while a plan holds their physical/code pins.
 * It adds no operation or allocation to ordinary native method calls. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootWithDomainV1(
    const void*,const XL6_ForeignViewDescriptorV1*,const XL6_OwnershipDomainV1*,const XL6_CodeOwner*);
/* Cold continuation from an already accepted real capability. Borrows parent
 * and retains one independent child; no host Acquire or reopened admission.
 * NULL parent is an explicit externally-resident C promise. Failure consumes
 * neither the native owner nor parent. The ordinary Watch/domain/creator and
 * checked retirement contracts are identical to CreateRootWithDomainV1. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootFromLeaseV1(
    const void*,const XL6_ForeignViewDescriptorV1*,const XL6_OwnershipDomainV1*,XL6_CodeLease*);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewBorrowChildV1(const void*,XL6_ForeignView*,const XL6_ForeignViewDescriptorV1*,const XL6_CodeOwner*);
/* Exact owner-result projection during a live native call. The closed IR
 * proves the nominal root type. raw must equal that same live native root;
 * no second control record/watch is created for e.g. Widget.context. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewBorrowRootV1(const void* raw,XL6_ForeignView* source);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewRetainV1(XL6_ForeignView*);
/* Consumes its language reference on success AND checked cleanup failure.
 * A void native drop adapter must propagate a false result's separate failure
 * bit, including when no diagnostic could be allocated. */
XL6_CODE_LEASE_API bool xl6ForeignViewReleaseV1(XL6_ForeignView*);
XL6_CODE_LEASE_API bool xl6ForeignViewCallBeginV1(XL6_ForeignView*,XL6_ForeignViewCallV1*,void*,size_t);
XL6_CODE_LEASE_API bool xl6ForeignViewCallEndV1(XL6_ForeignViewCallV1*);
XL6_CODE_LEASE_API bool xl6ForeignViewCloseV1(XL6_ForeignView*);
/* Consuming first-error delivery for native callbacks without a boundary.
 * A native callback record pins the root throughout publication. */
XL6_CODE_LEASE_API void xl6ForeignViewRecordCallbackFailureV1(XL6_ForeignView*,struct xl6_foreign_error_result);
XL6_CODE_LEASE_API bool xl6ForeignViewThrowIfCallbackFailedV1(XL6_ForeignView*);
XL6_CODE_LEASE_API bool xl6ForeignViewCallbackFailureCountV1(XL6_ForeignView*,uint64_t*);
/* Internal native binding owner: keeps the control/code readable without
 * creating a language owning value or suppressing default close. A retained
 * callback uses this for its implicit context, not a hidden strong language
 * self-reference. It is still a REAL graph edge and must be traced. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewNativePinV1(XL6_ForeignView*);
XL6_CODE_LEASE_API void xl6ForeignViewNativeUnpinV1(XL6_ForeignView*);
/* Cold external-code export from an exact live owning root on its creator.
 * Does not retain the resource or control, and does not discover code in an
 * arbitrary child/closed/claimed root. True transfers one independent lease;
 * true with NULL records that this root was explicitly externally resident.
 * False clears output and transfers nothing. The caller already owns view. */
XL6_CODE_LEASE_API bool xl6ForeignViewRetainCodeLeaseV1(XL6_ForeignView*,XL6_CodeLease**);

/* Native invalidation only clears/queues; it cannot release the final code
 * dependency while the native notifier is still on the C stack. Drain runs
 * on the creator thread after an entire foreign call returns, or explicitly
 * from the host outside native callbacks. Wrong-thread cleanup stays queued
 * for checked recovery. A callback boundary itself never drains work. */
XL6_CODE_LEASE_API bool xl6ForeignViewDrainV1(void);
/* Read-only queue observation; thread==0 counts all creator threads. */
XL6_CODE_LEASE_API size_t xl6ForeignViewPendingV1(uint64_t thread);
/* Freeze-only physical anchors, not additional owning references. */
XL6_CODE_LEASE_API bool xl6ForeignViewDiscoverV1(const XL6_CodeOwner*,XL6_ModuleOwnershipVisit,void*);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignViewV1Get(void);
XL6_CODE_LEASE_API struct xrtownershipref xl6ForeignViewOwnershipV1(const XL6_ForeignView*);

#endif
