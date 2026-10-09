/* One actual host entry/read implementation, included by the API TU. The
 * white-box gate compiles these SAME state/lock/Count/Trace/admission bodies.
 * Module destruction and unmapping remain the host EndCall tail, not here. */
#include "src/api/xl_module_state.h"
#include "src/runtime/xl6_code_lease.h"
#include <limits.h>
#include <stdlib.h>
/* Actual host entry references, not a synthetic pause/root list. Storage
 * belongs to each public call's native stack; links/count change under the
 * API lock. A read boundary holds that lock through Freeze and publication. */
typedef struct XL6_ModuleCall {
    XL_Module* module;
    unsigned long long thread;
    struct XL6_ModuleCall* previous;
    struct XL6_ModuleCall* next;
} XL6_ModuleCall;
static XL6_ModuleCall* xl__moduleCalls;
static XL6_OwnershipReadScopeV1* xl__moduleRead;

/* Caller owns the API lock and XRT Freeze. Other native/generated threads
 * are not made safe by this thread's pause. The retained entry and CodeScope
 * slots remain real external roots; Count does not subtract them. */
static bool xlModuleReadQuiescent(const XL_Module* module)
{
    const XL6_ModuleCall* call;
    size_t calls = 0;
    if (!module->ownershipReadContract ||
        module->ownershipReadContract->size != sizeof(*module->ownershipReadContract) ||
        module->ownershipReadContract->abiTag != 0x584C364D524431ULL ||
        module->ownershipReadContract->trace != module->ownershipTrace ||
        !xl__moduleRead || xl__moduleRead->self != xl__moduleRead ||
        xl__moduleRead->thread != xrtThreadCurrentId()) return false;
    for (call = xl__moduleCalls; call; call = call->next) if (call->module == module) {
        if (call->thread != xl__moduleRead->thread || calls == SIZE_MAX) return false;
        ++calls;
    }
    return calls == module->activeCalls &&
        !xl6CodeOwnerScopeForeign(&module->codeOwnerGraph.base);
}

/* Physical host module identity: the caller handle and each actual code
 * acquisition are references. The retirement registry is a discovery anchor,
 * not another owner. Entry/cleanup transactions are deliberately uninspectable.
 * Caller must hold the API lifecycle boundary AND whole-graph quiescence;
 * taking this view does not pause workers or authorize reclaiming code. */
static bool xlModuleOwnershipCount(const void* data, size_t* count)
{
    const XL_Module* module = (const XL_Module*)data;
    size_t handle;
    if (module == NULL || count == NULL || module->state == NULL ||
        !module->codeOwnerReady || module->ownershipTrace == NULL ||
        module->retirementRunning || module->ownershipCleared ||
        ((module->activeCalls != 0 || xl6CodeOwnerScopeActive(&module->codeOwnerGraph.base)) &&
            !xlModuleReadQuiescent(module)) ||
        module->activation == XL_MODULE_DEACTIVATING ||
        (module->activation == XL_MODULE_DEACTIVATION_PENDING &&
            !module->retirementPrepared)) return false;
    handle = module->destroyRequested ? 0u : 1u;
    if (module->activeCalls > SIZE_MAX - handle) return false;
    handle += module->activeCalls;
    if (module->codeLeases > SIZE_MAX - handle) return false;
    handle += module->codeLeases;
    if (module->preparationRefs > SIZE_MAX - handle) return false;
    handle += module->preparationRefs;
    if (module->snapshotRefs > SIZE_MAX - handle) return false;
    *count = handle + module->snapshotRefs; return true;
}
static bool xlModuleOwnershipTrace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL_Module* module = (const XL_Module*)data;
    size_t count;
    if (visit == NULL || !xlModuleOwnershipCount(data, &count)) return false;
    if (!visit(xrtErrorOwnership(module->retirementError), context)) return false;
    return module->ownershipTrace(visit, context);
}
static const xrtownershipops xlModuleOwnershipOps = {xlModuleOwnershipCount, xlModuleOwnershipTrace};

static int xlModuleReadBegin(XL6_OwnershipReadScopeV1* scope)
{
    if (!scope || scope->self || scope->thread) return XL6_OWNERSHIP_READ_ERROR_V1;
    /* Never queue for a host mutex while a foreign callback may be waiting
     * for us or an ownership transition. No Freeze/mutation is held here. */
    if (!xlApiTryLock()) return XL6_OWNERSHIP_READ_BUSY_V1;
    if (xl__moduleRead) { xlApiUnlock(); return XL6_OWNERSHIP_READ_BUSY_V1; }
    scope->self = scope; scope->thread = xrtThreadCurrentId();
    xl__moduleRead = scope; return XL6_OWNERSHIP_READ_READY_V1;
}
static bool xlModuleReadAdmits(const XL6_OwnershipReadScopeV1* scope,xrtownershipref ref)
{
    const XL_Module* module;
    size_t count;
    if (scope != xl__moduleRead || !scope || scope->self != scope ||
        scope->thread != xrtThreadCurrentId() || ref.Ops != &xlModuleOwnershipOps || !ref.Data) return false;
    module = ref.Data;
    return module->retirementContract && xlModuleReadQuiescent(module) &&
        xlModuleOwnershipCount(module,&count);
}
static void xlModuleReadEnd(XL6_OwnershipReadScopeV1* scope)
{
    if (!scope || scope != xl__moduleRead || scope->self != scope ||
        scope->thread != xrtThreadCurrentId()) abort();
    xl__moduleRead = NULL; *scope = (XL6_OwnershipReadScopeV1){0}; xlApiUnlock();
}
/* Separately bound resident host capability. Never reinterpret the stable
 * CodeOwnerGraph tail, accept source callbacks or reuse retirement admission.
 * Actual generated binding/plan selection is a separate compiler cutover. */
const XL6_OwnershipReadBoundaryV1* xlModuleOwnershipReadBoundaryV1(void)
{
    static const XL6_OwnershipReadBoundaryV1 boundary = {sizeof(boundary),
        xlModuleReadBegin,xlModuleReadAdmits,xlModuleReadEnd};
    return &boundary;
}

static int xlModuleBeginCallUnlocked(XL_Module* module)
{
    if (module == NULL || module->destroyRequested || module->activation == XL_MODULE_DEACTIVATING ||
        module->activeCalls == UINT_MAX) {
        return 0;
    }
    module->activeCalls += 1;
    return 1;
}
static void xlModuleCallAttachUnlocked(XL_Module* module, XL6_ModuleCall* call)
{
    if (!module || !module->activeCalls || !call || call->module || call->thread || call->previous || call->next) abort();
    call->module = module; call->thread = xrtThreadCurrentId();
    call->next = xl__moduleCalls;
    if (xl__moduleCalls) xl__moduleCalls->previous = call;
    xl__moduleCalls = call;
}

static void xlModuleCallDetachUnlocked(XL_Module* module, XL6_ModuleCall* call)
{
    XL6_ModuleCall* top;
    if (!module || !call || call->module != module || !module->activeCalls ||
        call->thread != xrtThreadCurrentId()) abort();
    for (top = xl__moduleCalls; top && top->thread != call->thread; top = top->next) {}
    if (top != call) abort();
    if (call->previous) call->previous->next = call->next;
    else xl__moduleCalls = call->next;
    if (call->next) call->next->previous = call->previous;
    *call = (XL6_ModuleCall){0};
}
