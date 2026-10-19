#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "xl6_closure.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_cell.h"
#include "xl6_ownership_scope.h"
#include <stdlib.h>
#include <string.h>

typedef struct ClosureStorage {
    XL6_Closure base; /* Preserve the public invocation prefix and V1 ABI. */
    const XL6_ClosureEnvironmentV1* layout;
    const XL6_ClosureEnvironmentOpsV1* environmentOps;
    const void* environmentLayout;
    size_t environmentBytes;
    const void* claim;
    void* retiredEnvironment;
    bool coordinated, cleared;
} ClosureStorage;
static void closure_release(XL6_Closure*);
static xrtownershipref closure_ownership(const XL6_Closure*);

static bool environment_valid(const XL6_ClosureEnvironmentV1* layout, const void* environment)
{
    if (!layout) return environment == NULL;
    if (!environment || layout->size != sizeof(*layout) || !layout->bytes ||
        !layout->count || !layout->slots) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_ClosureCaptureSlot* slot = &layout->slots[i];
        if (slot->kind < XL6_CLOSURE_CAPTURE_TRIVIAL || slot->kind > XL6_CLOSURE_CAPTURE_CELL ||
            !slot->bytes || slot->offset > layout->bytes || slot->bytes > layout->bytes - slot->offset ||
            (slot->kind == XL6_CLOSURE_CAPTURE_STRING && slot->bytes != sizeof(xstrview)) ||
            (slot->kind != XL6_CLOSURE_CAPTURE_TRIVIAL &&
             slot->kind != XL6_CLOSURE_CAPTURE_STRING && slot->bytes != sizeof(void*))) return false;
        for (size_t j = 0; j < i; ++j) {
            const XL6_ClosureCaptureSlot* prior = &layout->slots[j];
            if (slot->offset < prior->offset + prior->bytes && prior->offset < slot->offset + slot->bytes)
                return false;
        }
    }
    return true;
}
static bool environment_trace(const XL6_ClosureEnvironmentV1* layout, const void* environment,
    xrtownershipvisitor visit, void* context)
{
    if (!environment) return true;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_ClosureCaptureSlot* slot = &layout->slots[i];
        void* item; xrtownershipref ref;
        if (slot->kind == XL6_CLOSURE_CAPTURE_TRIVIAL || slot->kind == XL6_CLOSURE_CAPTURE_STRING) continue;
        memcpy(&item, (const char*)environment + slot->offset, sizeof(item));
        switch (slot->kind) {
        case XL6_CLOSURE_CAPTURE_XVALUE: ref = xrtValueOwnership(item); break;
        case XL6_CLOSURE_CAPTURE_FUTURE: ref = xrtFutureOwnership(item); break;
        case XL6_CLOSURE_CAPTURE_CLOSURE: ref = closure_ownership(item); break;
        case XL6_CLOSURE_CAPTURE_CELL: ref = xl6CellRuntimeV1Get()->Ownership(item); break;
        default: return false;
        }
        if (!visit(ref, context)) return false;
    }
    return true;
}
static void environment_drop_body(const XL6_ClosureEnvironmentV1* layout, void* environment)
{
    if (!environment) return;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_ClosureCaptureSlot* slot = &layout->slots[i];
        void* item;
        if (slot->kind == XL6_CLOSURE_CAPTURE_TRIVIAL) continue;
        if (slot->kind == XL6_CLOSURE_CAPTURE_STRING) {
            xstrview text;
            memcpy(&text, (char*)environment + slot->offset, sizeof(text));
            xrtFree((void*)text.Data);
            continue;
        }
        memcpy(&item, (char*)environment + slot->offset, sizeof(item));
        switch (slot->kind) {
        case XL6_CLOSURE_CAPTURE_XVALUE: xrtValueRelease(item); break;
        case XL6_CLOSURE_CAPTURE_FUTURE: xrtFutureDestroy(item); break;
        case XL6_CLOSURE_CAPTURE_CLOSURE: closure_release(item); break;
        case XL6_CLOSURE_CAPTURE_CELL: xl6CellRuntimeV1Get()->Release(item); break;
        default: abort();
        }
    }
    xrtFree(environment);
}
static void environment_drop(const XL6_ClosureEnvironmentV1* layout, void* environment)
{
    if (!environment_valid(layout, environment)) abort();
    /* Caller transfers a private environment; each child release coordinates
     * its own terminal transition, without enclosing semantic finalizers. */
    environment_drop_body(layout, environment);
}

static XL6_Closure* xl6OwnershipBody_closure_retain(XL6_Closure* closure)
{
    if (closure != NULL && xrtRefRetain(&closure->refs) < 0) abort();
    return closure;
}
static XL6_Closure* closure_retain(XL6_Closure* closure)
{
    XL6_OWNERSHIP_RETURN(XL6_Closure*, xl6OwnershipBody_closure_retain(closure));
}
static XL6_Closure* xl6OwnershipBody_closure_create(const XL6_CodeOwner* owner, void (*invoke)(void),
    void (*raw)(void), void* environment, void (*drop)(void*),
    const char* const* names, const xlir_call_default* defaults, size_t count, XL6_CodeContextTrace trace,
    const XL6_ClosureEnvironmentV1* layout, bool coordinated,
    const XL6_ClosureEnvironmentOpsV1* environmentOps,const void* environmentLayout,size_t environmentBytes)
{
    XL6_Closure* closure; XL6_CodeLease* lease = NULL; XL6_CodeLeaseAdmission admission;
    xerror* prior = xrtTakeError();
    if (invoke == NULL || (coordinated ? (environmentOps ?
            (environmentOps->size!=sizeof(*environmentOps) || !environmentOps->Validate ||
             !environmentOps->Trace || !environmentOps->Drop || !environmentLayout ||
             !environmentOps->Validate(environmentLayout,environment,environmentBytes)) : !environment_valid(layout, environment)) :
            (environment != NULL && (drop == NULL || trace == NULL)))) {
        xrtSetErrorKind(XERR_ARGUMENT); closure = NULL; goto done;
    }
    admission = xl6CodeLeaseAcquireScoped(owner, &lease);
    if (admission != XL6_CODE_LEASE_ACQUIRED) {
        if (xrtGetError() == NULL) xrtSetErrorKind(admission == XL6_CODE_LEASE_CLOSED ? XERR_CLOSED :
            admission == XL6_CODE_LEASE_RESOURCE_FAILURE ? XERR_MEMORY : XERR_ARGUMENT);
        closure = NULL; goto done;
    }
    closure = (XL6_Closure*)xrtCalloc(1, sizeof(ClosureStorage));
    if (closure == NULL) { xl6CodeLeaseRelease(lease); goto done; }
    *closure = (XL6_Closure){0}; closure->refs = 1;
    closure->invoke = invoke; closure->raw_callback = raw;
    closure->environment = environment; closure->environment_free = drop;
    closure->call_parameter_names = names; closure->call_parameter_defaults = defaults;
    closure->call_parameter_count = count; closure->environment_trace = trace; closure->lease = lease;
    ((ClosureStorage*)closure)->layout = layout;
    ((ClosureStorage*)closure)->environmentOps=environmentOps;
    ((ClosureStorage*)closure)->environmentLayout=environmentLayout;
    ((ClosureStorage*)closure)->environmentBytes=environmentBytes;
    ((ClosureStorage*)closure)->coordinated = coordinated;
done:
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return closure;
}
static XL6_Closure* closure_create(const XL6_CodeOwner* owner, void (*invoke)(void),
    void (*raw)(void), void* environment, void (*drop)(void*),
    const char* const* names, const xlir_call_default* defaults, size_t count, XL6_CodeContextTrace trace)
{
    XL6_OWNERSHIP_RETURN(XL6_Closure*, xl6OwnershipBody_closure_create(owner, invoke, raw, environment, drop, names, defaults, count, trace, NULL, false,NULL,NULL,0));
}
static XL6_Closure* closure_create_v2(const XL6_CodeOwner* owner, void (*invoke)(void),
    void (*raw)(void), void* environment, const XL6_ClosureEnvironmentV1* layout,
    const char* const* names, const xlir_call_default* defaults, size_t count)
{
    XL6_OWNERSHIP_RETURN(XL6_Closure*, xl6OwnershipBody_closure_create(owner, invoke, raw, environment,
        NULL, names, defaults, count, NULL, layout, true,NULL,NULL,0));
}
XL6_Closure* xl6ClosureCreateEnvironmentV1(const XL6_CodeOwner* owner,void (*invoke)(void),
    void (*raw)(void),void* environment,size_t bytes,const void* layout,const XL6_ClosureEnvironmentOpsV1* ops,
    const char* const* names,const xlir_call_default* defaults,size_t count)
{
    if(!ops){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(XL6_Closure*,xl6OwnershipBody_closure_create(owner,invoke,raw,environment,NULL,
        names,defaults,count,NULL,NULL,true,ops,layout,bytes));
}
static XL6_Closure* closure_create_value_body(const XL6_CodeOwner* owner, void (*invoke)(void), const xvalue* source)
{
    static const XL6_ClosureCaptureSlot slot = {XL6_CLOSURE_CAPTURE_XVALUE, 0, sizeof(xvalue*)};
    static const XL6_ClosureEnvironmentV1 layout = {sizeof(layout), sizeof(xvalue*), 1, &slot};
    xvalue** environment = NULL; XL6_Closure* closure = NULL; xerror* prior = xrtTakeError();
    if (!source || !invoke) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    environment = xrtMalloc(sizeof(*environment));
    if (!environment) goto done;
    *environment = xrtValueRetain(source);
    if (*environment) closure = xl6OwnershipBody_closure_create(owner, invoke, NULL, environment,
        NULL, NULL, NULL, 0, NULL, &layout, true,NULL,NULL,0);
    if (!closure) {
        xerror* failure = xrtTakeError(); environment_drop_body(&layout, environment);
        xrtClearError(); xrtSetErrorTake(failure);
    }
done:
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
    return closure;
}
static XL6_Closure* closure_create_value(const XL6_CodeOwner* owner, void (*invoke)(void), const xvalue* source)
{
    XL6_OWNERSHIP_RETURN(XL6_Closure*, closure_create_value_body(owner, invoke, source));
}
static void closure_release(XL6_Closure* closure)
{
    int32 refs; XL6_CodeLease* lease; XL6_CodeScope scope = {0};
    xrtownershipscope mutation = {0};
    void* environment; void (*drop)(void*); const XL6_ClosureEnvironmentV1* layout;
    const XL6_ClosureEnvironmentOpsV1* environmentOps;const void* environmentLayout;
    size_t environmentBytes;
    ClosureStorage* storage = (ClosureStorage*)closure; bool coordinated;
    if (closure == NULL) return;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    refs = xrtRefRelease(&closure->refs);
    if (refs < 0) abort();
    if (refs != 0) { if (!xrtOwnershipScopeEnd(&mutation)) abort(); return; }
    lease = closure->lease; environment = closure->environment; drop = closure->environment_free;
    if (storage->retiredEnvironment != NULL) abort(); /* Finish must precede final unpin. */
    layout = storage->layout; coordinated = storage->coordinated;
    environmentOps=storage->environmentOps;environmentLayout=storage->environmentLayout;
    environmentBytes=storage->environmentBytes;
    xl6CodeScopeEnter(&scope, lease);
    xrtFree(closure);
    /* The real code lease/scope protects descriptor reads and every return
     * tail. The zero-count closure no longer exposes a live graph node. */
    if (coordinated && !xrtOwnershipScopeEnd(&mutation)) abort();
    if(environmentOps){
        if(!environmentOps->Drop(environmentLayout,environment,environmentBytes)){
            /* Do not manufacture StateError to encode a false result. A
             * checked cleanup may fail without a diagnostic; Release is a
             * void boundary, so publish its independent failure bit to the
             * immediate scope and preserve the original (possibly NULL) error. */
            xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        }
    }
    else if (coordinated) environment_drop_body(layout, environment);
    else if (drop != NULL) drop(environment);
    xl6CodeLeaseRelease(lease);
    xl6CodeScopeLeave(&scope);
    if (!coordinated && !xrtOwnershipScopeEnd(&mutation)) abort();
}
static bool closure_count(const void* data, size_t* count)
{
    const XL6_Closure* closure = (const XL6_Closure*)data;
    int32 refs;
    /* Count is read only at the caller-provided whole-graph quiescent point. */
    if (closure == NULL || count == NULL || ((const ClosureStorage*)closure)->cleared ||
        (refs = closure->refs) <= 0) return false;
    *count = (size_t)refs; return true;
}
static bool closure_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_Closure* closure = (const XL6_Closure*)data;
    if (closure == NULL || visit == NULL || ((const ClosureStorage*)closure)->cleared) return false;
    if (closure->lease != NULL && !visit(xl6CodeLeaseOwnership(closure->lease), context)) return false;
    const ClosureStorage* storage=(const ClosureStorage*)closure;
    if(storage->environmentOps)
        return storage->environmentOps->Trace(storage->environmentLayout,closure->environment,storage->environmentBytes,visit,context);
    if (storage->coordinated)
        return environment_trace(((const ClosureStorage*)closure)->layout, closure->environment, visit, context);
    return closure->environment == NULL ||
        (closure->environment_trace != NULL && closure->environment_trace(closure->environment, visit, context));
}
static const xrtownershipops closure_ops = {closure_count, closure_trace};
static xrtownershipref closure_ownership(const XL6_Closure* closure)
{ return (xrtownershipref){closure, closure != NULL ? &closure_ops : NULL}; }
static void xl6OwnershipBody_closure_enter(XL6_ClosureScope* scope, XL6_Closure* closure)
{
    if (scope == NULL || scope->closure != NULL || closure == NULL || ((ClosureStorage*)closure)->cleared) abort();
    scope->closure = closure_retain(closure); xl6CodeScopeEnter(&scope->code, closure->lease);
}
static void closure_enter(XL6_ClosureScope* scope, XL6_Closure* closure)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_closure_enter(scope, closure));
}
static void closure_leave(XL6_ClosureScope* scope)
{
    XL6_Closure* closure;
    xrtownershipscope mutation = {0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    if (scope == NULL || scope->closure == NULL) abort();
    closure = scope->closure; scope->closure = NULL;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    closure_release(closure); xl6CodeScopeLeave(&scope->code);
}
static void closure_environment_retain(void* data) { (void)closure_retain((XL6_Closure*)data); }
static void closure_environment_release(void* data) { closure_release((XL6_Closure*)data); }
static void closure_payload_drop(void* value, void* unused)
{
    XL6_Closure* closure; (void)unused;
    xrtownershipscope mutation = {0};
    if (value == NULL) return;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    closure = *(XL6_Closure**)value; xrtFree(value);
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    closure_release(closure);
}
static bool closure_payload_trace(const void* value, const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{ return data == NULL && visit != NULL && (value == NULL || visit(closure_ownership(*(XL6_Closure* const*)value), context)); }

typedef struct ClosureBox {
    XL6_Closure* closure;
    XL6_CodeLease* adapter;
    XL6_ClosureBoxEntry entry;
} ClosureBox;
static bool box_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const ClosureBox* box = (const ClosureBox*)data;
    return visit(closure_ownership(box->closure), context) &&
        (box->adapter == NULL || visit(xl6CodeLeaseOwnership(box->adapter), context));
}
static bool box_invoke(void* data, const xrtcallframe* frame, xrtcallresult* result)
{
    ClosureBox* box = (ClosureBox*)data;
    XL6_CodeScope adapter = {0}; XL6_ClosureScope invocation = {0}; bool ok;
    xl6CodeScopeEnter(&adapter, box->adapter);
    closure_enter(&invocation, box->closure);
    ok = box->entry(box->closure, frame, result);
    closure_leave(&invocation); xl6CodeScopeLeave(&adapter);
    return ok;
}
static void box_drop(void* data)
{
    xrtownershipscope mutation = {0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    ClosureBox* box = (ClosureBox*)data;
    XL6_Closure* closure = box->closure; XL6_CodeLease* lease = box->adapter;
    XL6_CodeScope scope = {0}; xl6CodeScopeEnter(&scope, lease);
    xrtFree(box);
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    closure_release(closure); xl6CodeLeaseRelease(lease); xl6CodeScopeLeave(&scope);
}
static const xrtcallableownershipv1 box_ownership_policy = {
    sizeof(xrtcallableownershipv1), box_trace, box_drop
};
static xvalue* xl6OwnershipBody_closure_box(const XL6_CodeOwner* owner, XL6_Closure* closure, XL6_ClosureBoxEntry entry, uint64_t typeId, bool erased)
{
    XL6_CodeLease* lease = NULL; XL6_CodeLeaseAdmission admission;
    ClosureBox* box; xrtcallable* callable; xvalue* value = NULL; xerror* prior = xrtTakeError();
    if (closure == NULL || entry == NULL || (!erased && typeId == 0)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    admission = xl6CodeLeaseAcquireScoped(owner, &lease);
    if (admission != XL6_CODE_LEASE_ACQUIRED) {
        if (xrtGetError() == NULL) xrtSetErrorKind(admission == XL6_CODE_LEASE_CLOSED ? XERR_CLOSED :
            admission == XL6_CODE_LEASE_RESOURCE_FAILURE ? XERR_MEMORY : XERR_ARGUMENT);
        goto done;
    }
    box = (ClosureBox*)xrtMalloc(sizeof(*box));
    if (box == NULL) { xl6CodeLeaseRelease(lease); goto done; }
    box->closure = closure_retain(closure); box->adapter = lease; box->entry = entry;
    callable = xrtCallableCreate(NULL, box_invoke, box, box_drop);
    if (callable == NULL) { box_drop(box); goto done; }
    if (!xrtCallableOwnershipBindV1(callable, &box_ownership_policy)) {
        xerror* failure = xrtTakeError(); xrtCallableUnref(callable); xrtClearError(); xrtSetErrorTake(failure); goto done;
    }
    value = xrtValueCallableTake(&callable);
    if (callable != NULL) { xerror* failure = xrtTakeError(); xrtCallableUnref(callable); xrtClearError(); xrtSetErrorTake(failure); }
    if (value != NULL && !erased && !xrtValueTypeIdBind(value, typeId)) {
        xerror* failure = xrtTakeError(); xrtValueRelease(value); value = NULL; xrtClearError(); xrtSetErrorTake(failure);
    }
done:
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return value;
}
static xvalue* closure_box(const XL6_CodeOwner* owner, XL6_Closure* closure, XL6_ClosureBoxEntry entry, uint64_t typeId)
{
    XL6_OWNERSHIP_RETURN(xvalue*, xl6OwnershipBody_closure_box(owner, closure, entry, typeId, false));
}
xvalue* xl6ClosureBoxErasedV1(const XL6_CodeOwner* owner, XL6_Closure* closure, XL6_ClosureBoxEntry entry)
{
    XL6_OWNERSHIP_RETURN(xvalue*, xl6OwnershipBody_closure_box(owner, closure, entry, 0, true));
}
const XL6_ClosureRuntimeV1* xl6ClosureRuntimeV1Get(void)
{
    static const XL6_ClosureRuntimeV1 api = {closure_create, closure_retain, closure_release, closure_ownership,
        closure_enter, closure_leave, closure_environment_retain, closure_environment_release,
        closure_payload_drop, closure_payload_trace, closure_box};
    return &api;
}
const XL6_ClosureRuntimeV2* xl6ClosureRuntimeV2Get(void)
{
    static const XL6_ClosureRuntimeV2 api = {closure_create_v2, environment_drop, closure_create_value};
    return &api;
}
static bool closure_hold(const void* data)
{
    XL6_Closure* closure = (XL6_Closure*)data;
    return closure->refs > 0 && closure->refs < INT32_MAX && xrtRefRetain(&closure->refs) > 0;
}
static void closure_drop(const void* data) { closure_release((XL6_Closure*)data); }
static bool closure_claim(const void* data, const void* token)
{
    ClosureStorage* storage = (ClosureStorage*)data;
    if (storage->cleared || (storage->claim && storage->claim != token)) return false;
    storage->claim = token; return true;
}
static void closure_restore(const void* data, const void* token)
{
    ClosureStorage* storage = (ClosureStorage*)data;
    if (storage->claim != token || storage->cleared) abort();
    storage->claim = NULL;
}
static void closure_clear(const void* data, const void* token)
{
    ClosureStorage* storage = (ClosureStorage*)data;
    if (storage->claim != token || storage->cleared) abort();
    storage->retiredEnvironment = storage->base.environment;
    storage->base.environment = NULL; storage->cleared = true;
    /* Keep the actual code acquisition until all descriptor reads and the
     * physical closure destruction finish, including rollback/finish tails. */
}
static bool closure_finish(const void* data, const void* token)
{
    ClosureStorage* storage = (ClosureStorage*)data; void* environment;
    if (storage->claim != token || !storage->cleared) abort();
    environment = storage->retiredEnvironment; storage->retiredEnvironment = NULL;
    if(storage->environmentOps)return storage->environmentOps->Drop(storage->environmentLayout,environment,storage->environmentBytes);
    environment_drop_body(storage->layout, environment);return true;
}
const xrtownershipadapterv1* xl6ClosureOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), closure_hold, closure_drop,
        closure_claim, closure_restore, NULL, closure_clear, closure_finish};
    const ClosureStorage* storage;
    if (!ref.Data) return NULL;
    if (ref.Ops != &closure_ops) return xrtCallableOwnershipAdapterV1(ref, &box_ownership_policy);
    storage = ref.Data;
    return storage->coordinated && !storage->cleared && storage->base.refs > 0 ? &adapter : NULL;
}
