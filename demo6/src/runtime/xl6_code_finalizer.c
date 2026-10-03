#define XRT_MODULE_MEMORY
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "xl6_code_lease.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"

typedef struct XL6_CodeFinalizer {
    XL6_CodeLease* lease;
    XL6_CodeFinalizeProc finalize;
    bool (*checked)(xvalue*, void*);
    void* context;
    XL6_CodeContextTrace trace;
    XL6_CodeContextRelease release;
    bool ownsLease;
} XL6_CodeFinalizer;

static bool code_finalizer_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_CodeFinalizer* record = (const XL6_CodeFinalizer*)data;
    if (record->ownsLease && record->lease != NULL && !visit(xl6CodeLeaseOwnership(record->lease), context)) return false;
    return record->context == NULL || record->trace(record->context, visit, context);
}
static xl6_error_result code_finalizer_run(xvalue* value, void* data)
{
    XL6_CodeFinalizer* record = (XL6_CodeFinalizer*)data;
    XL6_CodeScope scope = {0};
    xl6_error_scope outcome = {0};
    if (!xl6ErrorScopeBegin(&outcome)) abort();
    xl6CodeScopeEnter(&scope, record->lease);
    if (record->checked != NULL) {
        if (!record->checked(value, record->context)) xl6ErrorScopeCaptureFailure(&outcome);
    } else record->finalize(value, record->context);
    xl6CodeScopeLeave(&scope);
    /* Do not free the context or lease here. Backing fields may still execute
     * generated Drop, and a cursor may outlive this finalizer invocation. */
    return xl6ErrorScopeEnd(&outcome);
}
static void code_finalizer_invoke(xvalue* value, void* data)
{
    xl6_error_result outcome = code_finalizer_run(value, data);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
static bool code_finalizer_checked(xvalue* value, void* data)
{
    xl6_error_result outcome = code_finalizer_run(value, data);
    if (!outcome.ok) {
        if (xrtGetError() == NULL) xrtSetErrorTake(outcome.error);
        else xrtErrorFree(outcome.error);
    }
    return outcome.ok;
}
static void xl6OwnershipBody_code_finalizer_release(void* data)
{
    XL6_CodeFinalizer* record = (XL6_CodeFinalizer*)data;
    XL6_CodeLease* lease = record->lease;
    XL6_CodeContextRelease release = record->release;
    void* context = record->context;
    bool ownsLease = record->ownsLease;
    XL6_CodeScope scope = {0};
    xl6CodeScopeEnter(&scope, lease);
    xrtFree(record);
    if (release != NULL) release(context);
    /* Nothing from the generated callback/descriptor is accessed past here. */
    if (ownsLease) xl6CodeLeaseRelease(lease);
    xl6CodeScopeLeave(&scope);
}
static void code_finalizer_release(void* data)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_code_finalizer_release(data));
}
static bool xl6OwnershipBody_code_finalizer_bind(xvalue* value, XL6_CodeLease* parent,
    XL6_CodeFinalizeProc finalize, void* context,
    XL6_CodeContextTrace trace, XL6_CodeContextRelease release, bool prepared, bool ownsLease,
    bool (*checked)(xvalue*, void*))
{
    XL6_CodeFinalizer* record; xerror* prior; bool accepted = false;
    if (value == NULL || (finalize == NULL && checked == NULL) ||
        (context != NULL && (trace == NULL || release == NULL))) return false;
    prior = xrtTakeError();
    record = (XL6_CodeFinalizer*)xrtMalloc(sizeof(*record));
    if (record != NULL) {
        record->lease = ownsLease ? xl6CodeLeaseRetain(parent) : parent;
        record->ownsLease = ownsLease;
        record->finalize = finalize; record->checked = checked; record->context = context;
        record->trace = trace; record->release = release;
        accepted = prepared
            ? xrtValueObjectFinalizerPrepareOwned(value, code_finalizer_invoke,
                record, code_finalizer_trace, code_finalizer_release)
            : xrtValueObjectFinalizerBindOwned(value, code_finalizer_invoke,
                record, code_finalizer_trace, code_finalizer_release);
        if (!accepted) {
            XL6_CodeLease* lease = record->lease;
            xrtFree(record); if (ownsLease) xl6CodeLeaseRelease(lease);
        }
    }
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return accepted;
}
static bool code_finalizer_bind(xvalue* value, XL6_CodeLease* parent,
    XL6_CodeFinalizeProc finalize, void* context,
    XL6_CodeContextTrace trace, XL6_CodeContextRelease release, bool prepared, bool ownsLease,
    bool (*checked)(xvalue*, void*))
{
    XL6_OWNERSHIP_RETURN(bool, xl6OwnershipBody_code_finalizer_bind(value, parent, finalize, context, trace, release, prepared, ownsLease, checked));
}

bool xl6CodeFinalizerBind(xvalue* value, XL6_CodeLease* parent,
    XL6_CodeFinalizeProc finalize, void* context,
    XL6_CodeContextTrace trace, XL6_CodeContextRelease release)
{
    return code_finalizer_bind(value, parent, finalize, context, trace, release, false, true, NULL);
}

/* Copyable code capability and unique finalization duty are different owners.
 * XRT's shared lifetime node outlives every backing field/finalizer-context
 * callback. A class finalizer borrows that lease rather than duplicating its
 * owning graph edge; ordinary classes install no synthetic finalizer at all. */
static bool class_lifetime_trace(const void* data, xrtownershipvisitor visit, void* context)
{ return visit(xl6CodeLeaseOwnership((const XL6_CodeLease*)data), context); }
static void class_lifetime_release(void* data)
{ xl6CodeLeaseRelease((XL6_CodeLease*)data); }

static const xvalueobjectownershipv1 class_ownership_policy = {
    sizeof(xvalueobjectownershipv1), code_finalizer_invoke, code_finalizer_trace,
    code_finalizer_release, class_lifetime_trace, class_lifetime_release, code_finalizer_checked
};
const xrtownershipadapterv1* xl6ClassOwnershipAdapterV1(xrtownershipref ref)
{ return xrtValueObjectOwnershipAdapterV1(ref, &class_ownership_policy); }
typedef struct ClassDiscovery {
    const XL6_CodeOwner* owner;
    XL6_ModuleOwnershipVisit visit;
    void* context;
} ClassDiscovery;
static bool class_discover(xrtownershipref ref,const void* lifetime,void* data)
{
    const ClassDiscovery* discovery=data;
    /* Exact resident policy proves Lifetime UserData is this factory's raw
     * lease. Read only its immutable identity, never foreign host callbacks. */
    return !xl6CodeLeaseOwnerMatches(lifetime,discovery->owner) ||
        discovery->visit(ref,discovery->context);
}
bool xl6ClassOwnershipDiscoverV1(const XL6_CodeOwner* owner,XL6_ModuleOwnershipVisit visit,void* context)
{
    if(!owner || owner->size<sizeof(*owner) || !owner->context || !owner->acquire ||
        !owner->release || !xl6CodeOwnerGraphOps(owner) || !visit){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    ClassDiscovery discovery={owner,visit,context};
    return xrtValueObjectOwnershipDiscoverV1(&class_ownership_policy,class_discover,&discovery);
}
static bool class_identity_bind(xvalue* value, xvalueidentityhash hash, xvalueidentityequal equal)
{ return xrtValueObjectIdentityBindV1(value, hash, equal, &class_ownership_policy); }

static xvalue* xl6OwnershipBody_class_create(const XL6_CodeOwner* owner, XL6_CodeFinalizeProc finalize,
    bool (*checked)(xvalue*, void*))
{
    XL6_CodeLease* lease = NULL;
    XL6_CodeLeaseAdmission admission;
    xvalue* value = NULL;
    xerror* prior = xrtTakeError();
    xerror* failure = NULL;
    admission = xl6CodeLeaseAcquireScoped(owner, &lease);
    if (admission != XL6_CODE_LEASE_ACQUIRED) {
        if (xrtGetError() == NULL) xrtSetErrorKind(admission == XL6_CODE_LEASE_CLOSED ? XERR_CLOSED :
            admission == XL6_CODE_LEASE_RESOURCE_FAILURE ? XERR_MEMORY : XERR_ARGUMENT);
        goto done;
    }
    value = xrtValueObjectLifo();
    if (value != NULL) {
        XL6_CodeLease* borrowed = lease;
        bool accepted = xrtValueObjectLifetimeBindOwned(value, lease,
            class_lifetime_trace, class_lifetime_release);
        if (accepted) {
            lease = NULL; /* One real transfer into the copyable lifetime node. */
            accepted = finalize != NULL || checked != NULL
                ? code_finalizer_bind(value, borrowed, finalize, NULL, NULL, NULL, true, false, checked)
                : xrtValueObjectConstructionPrepare(value);
            if (accepted) accepted = xrtValueObjectOwnershipBindV1(value, &class_ownership_policy);
        }
        if (!accepted) {
            failure = xrtTakeError();
            xrtValueRelease(value); value = NULL;
            xrtClearError(); xrtSetErrorTake(failure);
        }
    }
    xl6CodeLeaseRelease(lease);
done:
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return value;
}
static xvalue* class_create(const XL6_CodeOwner* owner, XL6_CodeFinalizeProc finalize)
{
    XL6_OWNERSHIP_RETURN(xvalue*, xl6OwnershipBody_class_create(owner, finalize, NULL));
}
static xvalue* class_create_checked(const XL6_CodeOwner* owner, bool (*checked)(xvalue*, void*))
{
    XL6_OWNERSHIP_RETURN(xvalue*, xl6OwnershipBody_class_create(owner, NULL, checked));
}

static xvalue* xl6OwnershipBody_class_commit_take(xvalue* value)
{
    xerror* prior = xrtTakeError();
    xerror* failure;
    if (xrtValueObjectConstructionCommit(value)) {
        if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
        return value;
    }
    failure = xrtTakeError();
    xrtValueRelease(value);
    xrtClearError();
    if (prior != NULL) { xrtErrorFree(failure); failure = prior; }
    xrtSetErrorTake(failure);
    return NULL;
}
static xvalue* class_commit_take(xvalue* value)
{
    XL6_OWNERSHIP_RETURN(xvalue*, xl6OwnershipBody_class_commit_take(value));
}

const XL6_ClassRuntimeV1* xl6ClassRuntimeV1Get(void)
{
    static const XL6_ClassRuntimeV1 api = {class_create, class_commit_take};
    return &api;
}
const XL6_ClassRuntimeV2* xl6ClassRuntimeV2Get(void)
{
    static const XL6_ClassRuntimeV2 api = {class_create_checked, class_commit_take, class_identity_bind};
    return &api;
}
