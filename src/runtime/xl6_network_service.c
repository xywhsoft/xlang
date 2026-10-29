#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_network_service.h"

static bool services_empty(const XL6_NetworkServices* services)
{
    return services && services->phase == XL6_NETWORK_SERVICE_EMPTY &&
        services->engine == NULL && services->resolver == NULL;
}

static bool services_initialize(XL6_NetworkServices* services)
{
    if (!services) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (services->phase == XL6_NETWORK_SERVICE_OPEN)
        return services->engine != NULL && services->resolver != NULL;
    if (!services_empty(services)) { xrtSetErrorKind(XERR_STATE); return false; }
    services->phase = XL6_NETWORK_SERVICE_STARTING;
    /* Install each real owner immediately. Failure leaves a retireable state,
     * not a forgotten local pointer or an implicitly successful cleanup. */
    services->engine = xrtNetEngineCreate(NULL);
    if (!services->engine || !xrtNetEngineStart(services->engine)) goto failed;
    services->resolver = xrtNetResolverCreate(NULL);
    if (!services->resolver) goto failed;
    services->phase = XL6_NETWORK_SERVICE_OPEN;
    return true;
failed:
    services->phase = XL6_NETWORK_SERVICE_RETIRING;
    return false;
}

static XL6_ModuleRetireResult engine_wait_objects(xnetengine* engine)
{
    xdeadline deadline = xrtDeadlineAfter(5000000u);
    xnetenginestats stats;
    for (;;) {
        if (!xrtNetEngineStats(engine, &stats)) return XL6_MODULE_RETIRE_FAILED;
        if (stats.LiveObjects == 0) return XL6_MODULE_RETIRE_READY;
        if (xrtDeadlineExpired(deadline)) return XL6_MODULE_RETIRE_BUSY;
        xrtSleep(1);
    }
}

static XL6_ModuleRetireResult translate(xnetretireresult result)
{
    switch (result) {
    case XNET_RETIRE_READY: return XL6_MODULE_RETIRE_READY;
    case XNET_RETIRE_BUSY: return XL6_MODULE_RETIRE_BUSY;
    default: return XL6_MODULE_RETIRE_FAILED;
    }
}

static XL6_ModuleRetireResult services_retire(XL6_NetworkServices* services, bool wait)
{
    xnetretireresult result;
    if (!services) { xrtSetErrorKind(XERR_ARGUMENT); return XL6_MODULE_RETIRE_FAILED; }
    if (services_empty(services)) return XL6_MODULE_RETIRE_READY;
    if (services->phase == XL6_NETWORK_SERVICE_STARTING) return XL6_MODULE_RETIRE_BUSY;
    if (services->phase != XL6_NETWORK_SERVICE_OPEN && services->phase != XL6_NETWORK_SERVICE_RETIRING) {
        xrtSetErrorKind(XERR_STATE); return XL6_MODULE_RETIRE_FAILED;
    }
    services->phase = XL6_NETWORK_SERVICE_RETIRING;
    /* A pending resolver callback may still need the engine. Finish its
     * accepted work before attempting to close the dependent service. */
    if (services->resolver) {
        result = wait ? (xrtNetResolverDestroy(services->resolver) ? XNET_RETIRE_READY : XNET_RETIRE_ERROR)
            : xrtNetResolverTryDestroy(services->resolver);
        if (result != XNET_RETIRE_READY) return translate(result);
        services->resolver = NULL;
    }
    if (services->engine) {
        if (wait) {
            XL6_ModuleRetireResult waited = engine_wait_objects(services->engine);
            if (waited != XL6_MODULE_RETIRE_READY) return waited;
        }
        result = wait ? (xrtNetEngineDestroy(services->engine) ? XNET_RETIRE_READY : XNET_RETIRE_ERROR)
            : xrtNetEngineTryDestroy(services->engine);
        if (result != XNET_RETIRE_READY) return translate(result);
        services->engine = NULL;
    }
    services->phase = XL6_NETWORK_SERVICE_EMPTY;
    return XL6_MODULE_RETIRE_READY;
}

static bool services_trace(const XL6_NetworkServices* services,
    XL6_ModuleOwnershipVisit visit, void* context)
{
    if (!services || !visit) return false;
    switch (services->phase) {
    case XL6_NETWORK_SERVICE_EMPTY:
        return services->engine == NULL && services->resolver == NULL;
    case XL6_NETWORK_SERVICE_STARTING:
        return false;
    case XL6_NETWORK_SERVICE_OPEN:
        if (!services->engine || !services->resolver) return false;
        break;
    case XL6_NETWORK_SERVICE_RETIRING:
        /* Initialization can fail before installing an engine. A resolver can
         * never outlive its engine slot, while an engine-only tail is normal. */
        if (services->resolver && !services->engine) return false;
        break;
    default:
        return false;
    }
    /* Preserve service retirement order. These are the actual creator slots,
     * not inferred worker/object counts or borrowed native handles. */
    if (services->resolver &&
        !visit(xrtNetResolverOwnership(services->resolver), context)) return false;
    if (services->engine &&
        !visit(xrtNetEngineOwnership(services->engine), context)) return false;
    return true;
}

const XL6_NetworkServiceRuntimeV1* xl6NetworkServiceRuntimeV1Get(void)
{
    static const XL6_NetworkServiceRuntimeV1 api = {
        services_initialize, services_retire, services_empty
    };
    return &api;
}

const XL6_NetworkServiceRuntimeV2* xl6NetworkServiceRuntimeV2Get(void)
{
    static const XL6_NetworkServiceRuntimeV2 api = {
        services_initialize, services_retire, services_empty, services_trace
    };
    return &api;
}
