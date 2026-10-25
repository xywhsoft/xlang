#include "xl6_foreign_resource.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_ownership_scope.h"
#include <stdlib.h>

typedef struct ForeignResource {
    void* raw;
    xvalue* releaser;
    xvalue* argument;
} ForeignResource;

static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const ForeignResource* resource=data;
    return visit && visit(xrtValueOwnership(resource->releaser),context) &&
        visit(xrtValueOwnership(resource->argument),context);
}
static bool emptyTrace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data;(void)context;return visit!=NULL; }
static void emptyRelease(void* data) { (void)data; }
static void release(void* data)
{
    ForeignResource* resource=data;
    xrtValueRelease(resource->argument);
    xrtValueRelease(resource->releaser);
    xrtFree(resource);
}
static bool finalizeChecked(xvalue* object,void* data)
{
    (void)object;
    ForeignResource* resource=data;
    if(!resource->raw)return true;
    /* Commit the once-only duty before invoking arbitrary language code. */
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))return false;
    resource->raw=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope))return false;
    xvalue* arguments[1]={resource->argument};
    xrtcallframe frame={.ArgumentCount=1,.Arguments=arguments};
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    bool ok=xrtValueInvoke(resource->releaser,&frame,&result);
    if(ok && xrtCallResultCount(&result)!=0){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xrtCallResultUnit(&result);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static void finalize(xvalue* object,void* data)
{ (void)finalizeChecked(object,data); }
static const xvalueobjectownershipv1 policy={sizeof(policy),finalize,emptyTrace,
    emptyRelease,trace,release,finalizeChecked};

xvalue* xl6ForeignResourceCreateV1(void* raw,const xvalue* releaser)
{
    if(!raw || !releaser || !xrtValueIsCallable(releaser)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    ForeignResource* resource=xrtCalloc(1,sizeof(*resource));
    if(!resource)return NULL;
    resource->argument=xrtValuePointer(raw);
    resource->releaser=xrtValueRetain((xvalue*)releaser);
    xvalue* token=xrtValueObject();
    if(!resource->argument || !resource->releaser || !token){
        xrtValueRelease(token);release(resource);return NULL;
    }
    if(!xrtValueObjectLifetimeBindOwned(token,resource,trace,release)){
        xrtValueRelease(token);release(resource);return NULL;
    }
    /* raw is committed only after every fallible binding. Failed construction
     * must not call the releaser for the caller-owned pointer. */
    if(!xrtValueObjectFinalizerPrepareOwned(token,finalize,resource,emptyTrace,emptyRelease) ||
       !xrtValueObjectOwnershipBindV1(token,&policy) ||
       !xrtValueObjectFinalizerCommit(token)){
        xrtValueRelease(token);return NULL;
    }
    resource->raw=raw;
    return token;
}
static bool pointer(const xvalueobjectreadv1* view,const xvalueobjectreadv1* unused,void* output)
{
    (void)unused;
    const ForeignResource* resource=xrtValueObjectReadLifetimeV1(view);
    if(!resource || !resource->raw){xrtSetErrorKind(XERR_CLOSED);return false;}
    *(void**)output=resource->raw;return true;
}
void* xl6ForeignResourcePointerV1(const xvalue* token)
{
    void* raw=NULL;
    return xrtValueObjectReadV1(token,NULL,&policy,pointer,&raw) ? raw : NULL;
}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtValueObjectOwnershipAdapterV1(ref,&policy);return *adapter!=NULL;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignResourceV1Get(void)
{
    static const XL6_OwnershipDomainV1 domain={sizeof(domain),NULL,resolve,NULL};return &domain;
}
