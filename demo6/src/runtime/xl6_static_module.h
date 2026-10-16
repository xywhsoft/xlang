#ifndef XL6_STATIC_MODULE_H
#define XL6_STATIC_MODULE_H
#include "xl6_ownership_plan.h"
#include "xl6_ownership_read_abi.h"

typedef struct XL6_StaticModule XL6_StaticModule;
/* Immutable deployment metadata, not language type inference. The caller's
 * exact policy resolver refuses unknown nodes BEFORE Count/Trace. Metadata,
 * callbacks and context outlive the record, including every failed retry. */
typedef struct XL6_StaticModuleContractV1 {
    size_t size;
    const XL6_ModuleRetirementV1* graph;
    /* Same persistent readiness duty as the host graph: no globals released,
     * no object finalizers, BUSY side-effect free. Modelled work has its own
     * graph preparation; a module must not wait for that work under a lock. */
    XL6_ModulePrepareResult (*preflight)(void);
    XL6_ModuleRetireResult (*step)(void);
    XL6_OwnershipResolveV3 resolve;
    void* context;
    const XL6_ModuleReadContractV1* read;
    bool (*admitsRead)(struct xrtownershipref,void*);
    bool (*discover)(const XL6_CodeOwner*,XL6_ModuleOwnershipVisit,void*);
} XL6_StaticModuleContractV1;
/* Copies the base contract before publication. The independent resident
 * probe distinguishes known transient states from unsupported nodes. */
typedef struct XL6_StaticModuleContractV2 {
    size_t size;
    const XL6_StaticModuleContractV1* base;
    XL6_OwnershipProbeV1 probe;
} XL6_StaticModuleContractV2;
typedef struct XL6_StaticModuleResultV1 {
    bool ready; /* Physical retirement finished; not merely an accepted close. */
    bool ok;    /* No failure event, including a failure without a diagnostic. */
} XL6_StaticModuleResultV1;

/* Create owns one real entry reference. Bind the borrowed owner before any
 * generated initialization; accepted descendants acquire actual code leases.
 * Retire consumes entry admission once, never another caller's references.
 * READY/FAILED are independent: recovery cannot erase the first failure.
 * Retire/Destroy require caller lifecycle exclusion; callbacks may reenter
 * Retire, which returns BUSY without running another driver. No user callback
 * runs while the record lock or an ownership freeze is held.
 *
 * Failed/BUSY retirement retains the exact record and plan. Keep its pointer
 * for retry; Destroy never frees an incomplete record. Before final Destroy,
 * stop/join all attempts to use the borrowed owner, just as for a JIT owner. */
XL6_CODE_LEASE_API XL6_StaticModule* xl6StaticModuleCreateV1(const XL6_StaticModuleContractV1*);
XL6_CODE_LEASE_API XL6_StaticModule* xl6StaticModuleCreateV2(const XL6_StaticModuleContractV2*);
XL6_CODE_LEASE_API const XL6_CodeOwner* xl6StaticModuleOwnerV1(const XL6_StaticModule*);
XL6_CODE_LEASE_API struct xrtownershipref xl6StaticModuleOwnershipV1(const XL6_StaticModule*);
XL6_CODE_LEASE_API XL6_StaticModuleResultV1 xl6StaticModuleRetireV1(XL6_StaticModule*);
/* Executable drain: yields while accepted execution/preparation makes the
 * graph temporarily busy, continues once-only semantic finalizer failures,
 * and returns an explicit non-ready result for external roots or a physical
 * failure requiring caller recovery. Never invents READY or drops a record. */
XL6_CODE_LEASE_API XL6_StaticModuleResultV1 xl6StaticModuleDrainV1(XL6_StaticModule*);
/* Wait for genuine external/native release tails too, never subtract them.
 * -1 waits until completion; 0 performs one bounded driver turn; a positive
 * timeout bounds retries between turns, not arbitrary user callbacks.
 * Unknown admission/physical failures return for checked recovery. Reentrant
 * same-thread calls return BUSY rather than waiting on their own callback. */
XL6_CODE_LEASE_API XL6_StaticModuleResultV1 xl6StaticModuleDrainForV1(XL6_StaticModule*,long long timeoutMilliseconds);
XL6_CODE_LEASE_API int xl6StaticModuleReadBeginV1(XL6_StaticModule*,XL6_OwnershipReadScopeV1*);
XL6_CODE_LEASE_API bool xl6StaticModuleReadAdmitsV1(const XL6_StaticModule*,const XL6_OwnershipReadScopeV1*,struct xrtownershipref);
XL6_CODE_LEASE_API void xl6StaticModuleReadEndV1(XL6_StaticModule*,XL6_OwnershipReadScopeV1*);
/* Consumes and NULLs only after physical retirement, irrespective of a prior
 * reported semantic failure. The caller must retain the checked result/error. */
XL6_CODE_LEASE_API bool xl6StaticModuleDestroyV1(XL6_StaticModule**);
#endif
