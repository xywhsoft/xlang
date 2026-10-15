#ifndef XL_MODULE_STATE_H
#define XL_MODULE_STATE_H
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_ownership_read.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_ownership_catalog.h"
typedef struct TCCState TCCState;
typedef struct XL_Module XL_Module;
struct XL_LoadedLibrary;

/* Actual host storage. Shared by lifecycle and read-only admission; no test
 * mirror, alternate count or implicit certification of executable storage. */
typedef enum XL_ModuleActivation {
    XL_MODULE_INACTIVE = 0,
    XL_MODULE_ACTIVE,
    XL_MODULE_DEACTIVATING,
    /* A failed checked finalizer may be busy, or may have retired resources
     * while reporting a cleanup error. Only a successful retry acknowledges
     * completion; neither case permits a fresh activation in between. */
    XL_MODULE_DEACTIVATION_PENDING
} XL_ModuleActivation;

struct XL_Module {
    TCCState* state;
    int ownsRuntimeRef;
    XL_ModuleActivation activation;
    struct XL_LoadedLibrary* loadedLibraries;
    size_t loadedLibraryCount;
    unsigned int activeCalls;
    int destroyRequested;
    XL6_CodeOwnerGraph codeOwnerGraph;
    XL6_ModuleOwnershipTrace ownershipTrace;
    const XL6_ModuleReadContractV1* ownershipReadContract;
    const XL6_ModuleRetirementV1* retirementContract;
    const XL6_ModuleRetirementV2* retirementPreparation;
    const XL6_ModuleRetirementV3* retirementPreflight;
    const XL6_ModuleRetirementV4* retirementStep;
    size_t preparationRefs;
    int retirementPrepared;
    XL6_OwnershipPlan* ownershipPlan;
    XL6_OwnershipCatalog* ownershipCatalog;
    const void* ownershipClaim;
    size_t snapshotRefs;
    int ownershipPhase;
    int ownershipCleared;
    size_t codeLeases;
    int codeOwnerReady;
    int retirementRunning;
    int retirementQueued;
    size_t retirementPass;
    XL_Module* retirementNext;
    xerror* retirementError;
    int retirementFailed; /* Failure is independent of diagnostic allocation. */
};
#endif
