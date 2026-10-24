#ifndef XL6_OWNERSHIP_CATALOG_H
#define XL6_OWNERSHIP_CATALOG_H
#include "xl6_ownership_plan.h"

/* Typed policy groups. No function-pointer casts, erased pointer arrays,
 * spelling tests, speculative Count/Trace or arbitrary population limit. */
#define XL6_OWNERSHIP_POLICY_GROUPS(X) \
    X(Sequence, XL6_SequenceAdmissionOpsV2) \
    X(Handle, XL6_NativeHandlePolicyV1) \
    X(Payload, xfuturepayloadownershipv1) \
    X(Producer, xfutureproducerownershipv1) \
    X(Watch, xfuturewatchownershipv1) \
    X(ProjectedWatch, xfuturewatchownershipv2) \
    X(CancelWatch, xcancelwatchownershipv1) \
    X(Combine, xfuturecombineownershipv1) \
    X(Task, xtaskdataownershipv1) \
    X(TaskNet, xtasknetdataownershipv1) \
    X(NetFileInput, xnetfiledataownershipv1) \
    X(QueuedStorage, XL6_ContinuationStorageV2) \
    X(ChannelSubscription, xchannelsubscribeownershipv1) \
    X(ResolverRequest, xnetresolveownershipv1) \
    X(DialDone, xnetdialownershipv1) \
    X(Quiescence, xrtownershipquiescencev1) \
    X(Executor, xexecutordataownershipv1) \
    X(ExecutionFamily, XL6_ExecutionFamilyV1) \
    X(ExecutorExit, xexecutorworkerexitownershipv1) \
    X(TaskPoolExit, xtaskpoolworkerexitownershipv1) \
    X(NetRuntime, xnetruntimeownershipv1) \
    X(ProxyDialDone, xnetproxydialownershipv1) \
    X(StreamEvents, xnetstreameventsownershipv1) \
    X(ListenerEvents, xnetlistenereventsownershipv1) \
    X(NetTask, xnettaskownershipv1) \
    X(NetTimer, xnettimerownershipv1) \
    X(NetWorkerExit, xnetengineworkerexitownershipv1) \
    X(TlsStreamEvents, xtlsstreamownershipv1) \
    X(TlsListenerEvents, xtlslistenerownershipv1) \
    X(TlsDialDone, xtlsdialownershipv1)
#define XL6_DECLARE_POLICY(Name, Type) struct Type;
XL6_OWNERSHIP_POLICY_GROUPS(XL6_DECLARE_POLICY)
#undef XL6_DECLARE_POLICY

typedef struct XL6_OwnershipCatalog XL6_OwnershipCatalog;
/* Independent exact-identity capability to recognize a valid native
 * transition. BUSY does not grant Count/Trace or lifecycle admission.
 * Frozen, read-only, resident, no callbacks/allocations/reference changes. */
typedef struct xrtownershipquiescencev1 XL6_OwnershipQuiescenceV1;
typedef struct XL6_OwnershipPoliciesV1 {
#define XL6_POLICY_VIEW(Name, Type) const struct Type* const* Name; size_t Name##Count;
    XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_VIEW)
#undef XL6_POLICY_VIEW
} XL6_OwnershipPoliciesV1;

typedef struct XL6_OwnershipDomainV1 {
    size_t size;
    /* Load executes only during creation, outside Freeze, before publication.
     * Policies and callbacks are borrowed immutable resident capabilities. */
    bool (*Load)(XL6_OwnershipCatalog*);
    /* Exact query only. false must leave all outputs NULL. The catalog checks
     * every domain and refuses conflicting admission instead of first-match
     * priority. Queries must not allocate or execute semantic callbacks. */
    bool (*Resolve)(struct xrtownershipref,const XL6_OwnershipPoliciesV1*,
        const struct xrtownershipadapterv1**,const struct xrtownershippreparationv1**,
        const XL6_OwnershipFinishV1**);
    /* Independent whole-graph READ capability, not implied by lifecycle
     * admission. NULL refuses opaque read participation. Frozen, no callbacks
     * through unknown node tables, no allocation or reference transitions. */
    bool (*AdmitsRead)(struct xrtownershipref);
} XL6_OwnershipDomainV1;

/* Create copies the domain pointer list and seals the policy population.
 * Descriptors, policies and callback code outlive the catalog and all plans.
 * Failure consumes nothing and releases every partial allocation. Lifecycle
 * exclusion is required for Create/Destroy. Resolve is read-only and may run
 * concurrently, but the caller must hold the physical graph's Freeze while
 * querying mutable nodes, as with every OwnershipPlan resolver. Borrowed node
 * lifetimes remain the caller's responsibility. Publication seals mutation. */
XL6_CODE_LEASE_API XL6_OwnershipCatalog* xl6OwnershipCatalogCreateV1(
    const XL6_OwnershipDomainV1* const*,size_t);
XL6_CODE_LEASE_API void xl6OwnershipCatalogDestroyV1(XL6_OwnershipCatalog*);
XL6_CODE_LEASE_API bool xl6OwnershipCatalogResolveV1(struct xrtownershipref,void*,
    const struct xrtownershipadapterv1**,const struct xrtownershippreparationv1**,
    const XL6_OwnershipFinishV1**);
XL6_CODE_LEASE_API bool xl6OwnershipCatalogAdmitsReadV1(struct xrtownershipref,void*);
XL6_CODE_LEASE_API XL6_OwnershipAdmissionV1 xl6OwnershipCatalogProbeV1(struct xrtownershipref,void*,
    const struct xrtownershipadapterv1**,const struct xrtownershippreparationv1**,
    const XL6_OwnershipFinishV1**);
/* Only a Load callback on its unpublished catalog may append a non-NULL
 * capability. Repeated identical capabilities are deduplicated by identity. */
#define XL6_POLICY_ADD(Name, Type) \
    XL6_CODE_LEASE_API bool xl6OwnershipCatalogAdd##Name##V1(XL6_OwnershipCatalog*,const struct Type*);
XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_ADD)
#undef XL6_POLICY_ADD
#endif
