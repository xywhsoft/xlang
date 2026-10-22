#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_GRAPH
#include "lib/xrt/xrt.h"
#include "xl6_code_lease.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

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
typedef struct ClassLifetimeV3 {
    XL6_CodeLease* lease;
    const XL6_ClassIdentitySchemaV3* schema;
} ClassLifetimeV3;
static bool class_lifetime_trace_v3(const void* data,xrtownershipvisitor visit,void* context)
{ return visit(xl6CodeLeaseOwnership(((const ClassLifetimeV3*)data)->lease),context); }
static void class_lifetime_release_v3(void* data)
{
    ClassLifetimeV3* lifetime=data; XL6_CodeLease* lease=lifetime->lease;
    xrtFree(lifetime); xl6CodeLeaseRelease(lease);
}
static const xvalueobjectownershipv1 class_ownership_policy_v3 = {
    sizeof(xvalueobjectownershipv1), code_finalizer_invoke, code_finalizer_trace,
    code_finalizer_release, class_lifetime_trace_v3, class_lifetime_release_v3, code_finalizer_checked
};
const xrtownershipadapterv1* xl6ClassOwnershipAdapterV1(xrtownershipref ref)
{
    const xrtownershipadapterv1* adapter=xrtValueObjectOwnershipAdapterV1(ref,&class_ownership_policy);
    return adapter ? adapter : xrtValueObjectOwnershipAdapterV1(ref,&class_ownership_policy_v3);
}
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
static bool class_discover_v3(xrtownershipref ref,const void* lifetime,void* data)
{
    const ClassDiscovery* discovery=data;
    /* Only exact V3 enrollment proves this is a ClassLifetimeV3, not a raw V2
     * lease or an arbitrary native pointer with superficially equal hooks. */
    return !xl6CodeLeaseOwnerMatches(((const ClassLifetimeV3*)lifetime)->lease,discovery->owner) ||
        discovery->visit(ref,discovery->context);
}
bool xl6ClassOwnershipDiscoverV1(const XL6_CodeOwner* owner,XL6_ModuleOwnershipVisit visit,void* context)
{
    if(!owner || owner->size<sizeof(*owner) || !owner->context || !owner->acquire ||
        !owner->release || !xl6CodeOwnerGraphOps(owner) || !visit){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    ClassDiscovery discovery={owner,visit,context};
    return xrtValueObjectOwnershipDiscoverV1(&class_ownership_policy,class_discover,&discovery) &&
        xrtValueObjectOwnershipDiscoverV1(&class_ownership_policy_v3,class_discover_v3,&discovery);
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

static bool class_schema_valid_v3(const XL6_ClassIdentitySchemaV3* schema)
{
    if (!schema || schema->size!=sizeof(*schema) || !schema->identity || !schema->runtimeTypeTag ||
        (unsigned)schema->hashMode>XL6_CLASS_IDENTITY_DERIVED_V3 ||
        (unsigned)schema->equalMode>XL6_CLASS_IDENTITY_DERIVED_V3 ||
        schema->fieldCount>SIZE_MAX/sizeof(*schema->fields) || (!schema->fields && schema->fieldCount)) return false;
    const XL6_ClassDynamicFieldPathV3* path=&schema->dynamicFields;
    if (path->storageName || path->storageSize || path->handleName || path->handleSize) {
        if (!path->storageName || !path->storageSize || !path->handleName || !path->handleSize) return false;
    }
    for (size_t i=0;i<schema->fieldCount;++i)
        if ((!schema->fields[i].name && schema->fields[i].nameSize) || !schema->fields[i].identity) return false;
    return true;
}
static xvalue* xl6OwnershipBody_class_create_v3(const XL6_CodeOwner* owner,
    bool (*checked)(xvalue*,void*),const XL6_ClassIdentitySchemaV3* schema,XL6_CodeLease* parent,bool derived)
{
    XL6_CodeLease* lease=NULL; ClassLifetimeV3* lifetime=NULL; xvalue* value=NULL;
    xerror* prior=xrtTakeError(); xerror* failure;
    if (!class_schema_valid_v3(schema)) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    XL6_CodeLeaseAdmission admission;
    if(derived){lease=xl6CodeLeaseRetain(parent);admission=(!parent||lease)?XL6_CODE_LEASE_ACQUIRED:XL6_CODE_LEASE_CLOSED;}
    else admission=xl6CodeLeaseAcquireScoped(owner,&lease);
    if (admission!=XL6_CODE_LEASE_ACQUIRED) {
        if (!xrtGetError()) xrtSetErrorKind(admission==XL6_CODE_LEASE_CLOSED ? XERR_CLOSED :
            admission==XL6_CODE_LEASE_RESOURCE_FAILURE ? XERR_MEMORY : XERR_ARGUMENT);
        goto done;
    }
    lifetime=xrtMalloc(sizeof(*lifetime)); if (!lifetime) goto done;
    *lifetime=(ClassLifetimeV3){lease,schema};
    value=xrtValueObjectLifo(); if (!value) goto done;
    XL6_CodeLease* borrowed=lease;
    if (!xrtValueObjectLifetimeBindOwned(value,lifetime,class_lifetime_trace_v3,class_lifetime_release_v3)) goto fail;
    lifetime=NULL; lease=NULL;
    if (!(checked ? code_finalizer_bind(value,borrowed,NULL,NULL,NULL,NULL,true,false,checked) :
            xrtValueObjectConstructionPrepare(value)) ||
        !xrtValueObjectOwnershipBindV1(value,&class_ownership_policy_v3)) goto fail;
    goto done;
fail:
    failure=xrtTakeError(); xrtValueRelease(value); value=NULL;
    xrtClearError(); xrtSetErrorTake(failure);
done:
    failure=xrtTakeError(); xrtFree(lifetime); xl6CodeLeaseRelease(lease);
    xrtClearError();
    if (prior) { xrtErrorFree(failure); xrtSetErrorTake(prior); } else xrtSetErrorTake(failure);
    return value;
}
static xvalue* class_create_v3(const XL6_CodeOwner* owner,bool (*checked)(xvalue*,void*),
    const XL6_ClassIdentitySchemaV3* schema)
{ XL6_OWNERSHIP_RETURN(xvalue*,xl6OwnershipBody_class_create_v3(owner,checked,schema,NULL,false)); }
xvalue* xl6ClassCreateFromLeaseV3(XL6_CodeLease* parent,const XL6_ClassIdentitySchemaV3* schema)
{ XL6_OWNERSHIP_RETURN(xvalue*,xl6OwnershipBody_class_create_v3(NULL,NULL,schema,parent,true)); }

/* No C TLS: TinyCC's PE writer cannot relocate it. Stack frames and short
 * native lanes bound class callback recursion, including recursive fields
 * reached through dynamic/sequence adapters. No user code runs under a lock. */
typedef struct ClassIdentityFrameV3 {
    struct ClassIdentityFrameV3* next;
    uint64 thread;
    unsigned depth;
} ClassIdentityFrameV3;
typedef struct ClassIdentityLaneV3 { xatomic32 lock; ClassIdentityFrameV3* active; } ClassIdentityLaneV3;
static ClassIdentityLaneV3 class_identity_lanes_v3[64];
static ClassIdentityLaneV3* class_identity_lane_v3(uint64 thread)
{ return &class_identity_lanes_v3[(thread*UINT64_C(0x9E3779B97F4A7C15))>>58]; }
static void class_identity_lock_v3(ClassIdentityLaneV3* lane)
{
    uint32 expected;
    do { expected=0; } while (!xrtAtomic32CompareExchange(&lane->lock,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED));
}
static bool class_identity_enter_v3(ClassIdentityFrameV3* frame)
{
    xrtownershipscope observation={0};
    if (!xrtOwnershipMutationBegin(&observation)) return false;
    frame->thread=observation.Thread;
    if (!xrtOwnershipScopeEnd(&observation)) abort();
    ClassIdentityLaneV3* lane=class_identity_lane_v3(frame->thread); class_identity_lock_v3(lane);
    ClassIdentityFrameV3* prior=lane->active;
    while (prior && prior->thread!=frame->thread) prior=prior->next;
    bool ready=!prior || prior->depth<XL6_CLASS_IDENTITY_DEPTH_MAX_V3;
    if (ready) { frame->depth=prior ? prior->depth+1 : 1; frame->next=lane->active; lane->active=frame; }
    xrtAtomic32Store(&lane->lock,0,XMEMORY_RELEASE);
    if (!ready) xrtSetErrorKind(XERR_RANGE);
    return ready;
}
static void class_identity_leave_v3(ClassIdentityFrameV3* frame)
{
    ClassIdentityLaneV3* lane=class_identity_lane_v3(frame->thread); class_identity_lock_v3(lane);
    ClassIdentityFrameV3** link=&lane->active;
    while (*link && (*link)->thread!=frame->thread) link=&(*link)->next;
    if (*link!=frame) abort(); /* Private stack discipline, never an operation outcome. */
    *link=frame->next; xrtAtomic32Store(&lane->lock,0,XMEMORY_RELEASE);
}
static const XL6_ClassIdentitySchemaV3* class_read_schema_v3(const xvalueobjectreadv1* view,bool fieldsReady)
{
    const ClassLifetimeV3* lifetime=xrtValueObjectReadLifetimeV1(view);
    int64 tag=0;
    if (!lifetime || !lifetime->schema || xrtValueObjectReadTypeIdV1(view)!=lifetime->schema->identity) {
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    /* IR binds immutable identity BEFORE initializing runtime-tag/field slots.
     * Binding certifies metadata, not construction completeness. Operations
     * which actually read contents require their own initialized-slot checks. */
    if (!fieldsReady) return lifetime->schema;
    const xvalue* field=xrtValueObjectReadGetV1(view,XRT_STR_LITERAL("$xlir.class.runtime-type"));
    if (!field) { xrtSetErrorKind(XERR_STATE); return NULL; }
    if (!xrtValueGetInt(field,&tag)) return NULL;
    if (tag!=(int64)lifetime->schema->runtimeTypeTag) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return lifetime->schema;
}
static bool class_dynamic_path_visit_v3(const xvalueobjectreadv1* view,
    const xvalueobjectreadv1* unused,void* data)
{
    (void)unused;
    const XL6_ClassIdentitySchemaV3* schema=class_read_schema_v3(view,false);
    if (!schema || !class_schema_valid_v3(schema)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        return false;
    }
    *(XL6_ClassDynamicFieldPathV3*)data=schema->dynamicFields;
    return true;
}
static bool class_ordinary_backing_visit_v3(xrtownershipref ref,void* output)
{
    /* A resident Object shell emits only its physical backing edge, not its
     * policy callbacks or children. A shell certificate alone is insufficient:
     * a class with language-defined identity may have no native identity hook. */
    *(bool*)output=xrtValueOwnershipAdapterV1(ref)!=NULL;
    return true;
}
int xl6ClassDynamicFieldPathV3(const xvalue* source,XL6_ClassDynamicFieldPathV3* output)
{
    if (!source || !output) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    xrtownershipref ref=xrtValueOwnership(source);
    /* Only recognized ordinary storage is a safe miss. ReadV1 authenticates
     * V3 itself, including prepared constructors that collector admission
     * deliberately refuses. Unknown policies/BUSY/claimed/cleared/corrupted
     * instances are checked failures, never a fallback to ordinary storage. */
    if (xrtValueType(source)!=XVALUE_OBJECT) { xrtSetErrorKind(XERR_TYPE); return -1; }
    if (xrtValueObjectOwnershipAdapterV1(ref,&class_ownership_policy)) return 0;
    bool ordinary=false;
    if (!ref.Ops->Trace(ref.Data,class_ordinary_backing_visit_v3,&ordinary)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        return -1;
    }
    if (ordinary) return 0;
    XL6_ClassDynamicFieldPathV3 path={0};
    if (!xrtValueObjectReadV1(source,NULL,&class_ownership_policy_v3,
            class_dynamic_path_visit_v3,&path)) return -1;
    if (!path.storageName) return 0;
    *output=path; return 1;
}
static bool class_schema_equal_v3(const XL6_ClassIdentitySchemaV3* a,const XL6_ClassIdentitySchemaV3* b)
{
    if (a==b) return true;
    if (a->identity!=b->identity || a->hashMode!=b->hashMode || a->equalMode!=b->equalMode || a->fieldCount!=b->fieldCount) return false;
    for (size_t i=0;i<a->fieldCount;++i) {
        const XL6_ClassIdentityFieldV3 *left=&a->fields[i],*right=&b->fields[i];
        if (left->identity!=right->identity || left->nameSize!=right->nameSize ||
            (left->nameSize && memcmp(left->name,right->name,left->nameSize))) return false;
    }
    return true;
}
typedef struct ClassIdentityReadV3 { const xvalue* left; const xvalue* right; bool equal; uint64 hash; } ClassIdentityReadV3;
static bool class_equal_visit_v3(const xvalueobjectreadv1* left,const xvalueobjectreadv1* right,void* data)
{
    ClassIdentityReadV3* result=data;
    const XL6_ClassIdentitySchemaV3* a=class_read_schema_v3(left,true);
    if (!a) return false;
    const XL6_ClassIdentitySchemaV3* b=class_read_schema_v3(right,true);
    if (!b) return false;
    if (!class_schema_equal_v3(a,b)) { result->equal=false; return true; }
    if (a->equalMode==XL6_CLASS_IDENTITY_REFERENCE_V3) { result->equal=result->left==result->right; return true; }
    if (a->equalMode!=XL6_CLASS_IDENTITY_DERIVED_V3) { xrtSetErrorKind(XERR_UNSUPPORTED); return false; }
    for (size_t i=0;i<a->fieldCount;++i) {
        const XL6_ClassIdentityFieldV3* field=&a->fields[i];
        xstrview key={field->name,field->nameSize};
        const xvalue *l=xrtValueObjectReadGetV1(left,key),*r=xrtValueObjectReadGetV1(right,key);
        if (!l || !r) { xrtSetErrorKind(XERR_STATE); return false; }
        bool equal=xrtValueEqual(l,r);
        if (xrtGetError()) return false;
        if (!equal) { result->equal=false; return true; }
    }
    result->equal=true; return true;
}
static bool class_equal_v3(const xvalue* left,const xvalue* right,bool* output)
{
    if (!output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    ClassIdentityFrameV3 frame={0}; if (!class_identity_enter_v3(&frame)) return false;
    ClassIdentityReadV3 result={.left=left,.right=right};
    bool ready=right && xrtValueObjectReadV1(left,right,&class_ownership_policy_v3,class_equal_visit_v3,&result);
    if (!right) xrtSetErrorKind(XERR_ARGUMENT);
    class_identity_leave_v3(&frame);
    if (ready) *output=result.equal;
    return ready;
}
static bool class_hash_visit_v3(const xvalueobjectreadv1* view,const xvalueobjectreadv1* unused,void* data)
{
    (void)unused; ClassIdentityReadV3* result=data;
    const XL6_ClassIdentitySchemaV3* schema=class_read_schema_v3(view,true); if (!schema) return false;
    if (schema->hashMode==XL6_CLASS_IDENTITY_REFERENCE_V3) { result->hash=(uint64)(uintptr_t)result->left; return true; }
    if (schema->hashMode!=XL6_CLASS_IDENTITY_DERIVED_V3) { xrtSetErrorKind(XERR_UNSUPPORTED); return false; }
    uint64 hash=schema->identity^UINT64_C(0x9E3779B97F4A7C15);
    for (size_t i=0;i<schema->fieldCount;++i) {
        const XL6_ClassIdentityFieldV3* field=&schema->fields[i];
        const xvalue* item=xrtValueObjectReadGetV1(view,(xstrview){field->name,field->nameSize}); uint64 itemHash=0;
        if (!item) { xrtSetErrorKind(XERR_STATE); return false; }
        if (!xrtValueHash(item,&itemHash) || xrtGetError()) return false;
        hash^=itemHash+UINT64_C(0x9E3779B97F4A7C15)+(hash<<6)+(hash>>2);
    }
    result->hash=hash; return true;
}
static uint64 class_hash_callback_v3(const xvalue* value,void* unused)
{
    (void)unused; ClassIdentityFrameV3 frame={0}; ClassIdentityReadV3 result={.left=value};
    bool ready=class_identity_enter_v3(&frame);
    if (ready) { ready=xrtValueObjectReadV1(value,NULL,&class_ownership_policy_v3,class_hash_visit_v3,&result); class_identity_leave_v3(&frame); }
    if (!ready) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); /* Allocation-free legacy XRT identity boundary. */
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    }
    return ready ? result.hash : 0;
}
static bool class_equal_callback_v3(const xvalue* left,const xvalue* right,void* unused)
{
    (void)unused; bool equal=false;
    if (!class_equal_v3(left,right,&equal)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        return false;
    }
    return equal;
}
static bool class_bind_visit_v3(const xvalueobjectreadv1* view,const xvalueobjectreadv1* unused,void* data)
{
    (void)unused; (void)data; const XL6_ClassIdentitySchemaV3* schema=class_read_schema_v3(view,false);
    if (!schema) return false;
    if (!((schema->hashMode==XL6_CLASS_IDENTITY_REFERENCE_V3 && schema->equalMode==XL6_CLASS_IDENTITY_REFERENCE_V3) ||
        (schema->hashMode==XL6_CLASS_IDENTITY_DERIVED_V3 && schema->equalMode==XL6_CLASS_IDENTITY_DERIVED_V3))) {
        xrtSetErrorKind(XERR_UNSUPPORTED); return false;
    }
    return true;
}
static xvalue* xl6OwnershipBody_class_bind_take_v3(xvalue* value)
{
    xerror* prior=xrtTakeError(); xerror* failure;
    if (xrtValueObjectReadV1(value,NULL,&class_ownership_policy_v3,class_bind_visit_v3,NULL) &&
        xrtValueObjectIdentityBindV1(value,class_hash_callback_v3,class_equal_callback_v3,&class_ownership_policy_v3)) {
        xrtClearError(); xrtSetErrorTake(prior); return value;
    }
    failure=xrtTakeError(); xrtValueRelease(value); xrtClearError();
    if (prior) { xrtErrorFree(failure); failure=prior; } xrtSetErrorTake(failure); return NULL;
}
static xvalue* class_bind_take_v3(xvalue* value)
{ XL6_OWNERSHIP_RETURN(xvalue*,xl6OwnershipBody_class_bind_take_v3(value)); }
const XL6_ClassRuntimeV3* xl6ClassRuntimeV3Get(void)
{
    static const XL6_ClassRuntimeV3 api={class_create_v3,class_commit_take,class_bind_take_v3,class_equal_v3}; return &api;
}
