#include "xl6_execution_resource_internal.h"
#include "xl6_execution_net_resolver.h"
#include "xl6_concurrent_deadline.h"
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Resolver {Resource base;xnetresolverconfig config;} Resolver;
static bool create(Resource* r,const XL6_ExecutionCreateOptionsV1* options)
{
    Resolver* resolver=(Resolver*)r;xrtNetResolverConfigInit(&resolver->config);
    if(options->value.resolver)resolver->config=*options->value.resolver;
    /* Arbitrary callback addresses and unowned user data never cross this family. */
    if(resolver->config.Lookup||resolver->config.LookupData){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    r->resolver=xrtNetResolverCreate(&resolver->config);return r->resolver!=NULL;
}
static bool rollback(Resource* r)
{if(r->resolver&&!xrtNetResolverDestroy(r->resolver))return false;r->resolver=NULL;return true;}
static XL6_ModulePrepareResult retire(Resource* r)
{
    if(!r->resolver)return XL6_MODULE_PREPARE_READY;
    xnetretireresult result=xrtNetResolverTryDestroy(r->resolver);
    return result==XNET_RETIRE_READY?XL6_MODULE_PREPARE_READY:
        result==XNET_RETIRE_BUSY?XL6_MODULE_PREPARE_BUSY:XL6_MODULE_PREPARE_FAILED;
}
static bool finish(Resource* r){return !r->resolver||xrtNetResolverDestroy(r->resolver);}
static void drop(Resource* r){(void)r;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return !r->resolver||visit(xrtNetResolverOwnership(r->resolver),context);}
/* Close only orders language admission; no native destruction may race a query. */
static bool close(Resource* r){(void)r;return true;}
static bool present(const Resource* r){return r->resolver!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionNetResolverFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,
        XL6_EXECUTION_NET_RESOLVER_V1,create,rollback,retire,finish,drop,trace,close,close,sizeof(Resolver),present};
    return &family;
}
Resource* xl6ExecutionNetResolverCreateV1(XL6_ExecutionSourceV1* source,const xnetresolverconfig* config)
{
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_NET_RESOLVER_V1,.value.resolver=config};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionNetResolverFamilyV1Get(),&options);
}
static bool enter(Resource* resource,bool submit,bool control,XL6_ExecutionScopeV1* scope)
{
    bool ok=control?xl6ExecutionResourceControlV1(resource,submit,scope):xl6ExecutionResourceEnterV1(resource,submit,scope);
    if(ok&&resource->kind!=XL6_EXECUTION_NET_RESOLVER_V1){xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return ok;
}
bool xl6ExecutionNetResolverWaitV1(Resource* resource,int64_t millis,bool* ready)
{
    if(!ready){xrtSetErrorKind(XERR_ARGUMENT);return false;}*ready=false;
    xdeadline deadline;if(!xl6ConcurrentDeadline(millis,&deadline))return false;
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,true,&scope))return false;bool ok=false;
    if(!xl6ExecutionRequestedInternalV1(resource))xrtSetErrorKind(XERR_STATE);
    else if(!scope.resolver){*ready=true;ok=true;}
    else for(;;){
        xnetretireresult status=xrtNetResolverTryDestroy(scope.resolver);
        if(status==XNET_RETIRE_ERROR)break;
        if(status==XNET_RETIRE_READY){xl6ExecutionResolverConsumedInternalV1(&scope);*ready=true;ok=true;break;}
        if(xrtDeadlineExpired(deadline)){ok=true;break;}
        xrtSleepUs(UINT64_C(1000));
    }
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetResolverClearV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,true,false,&scope))return false;
    bool ok=xrtNetResolverClear(scope.resolver);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetResolverConfigV1(Resource* resource,xnetresolverconfig* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    *output=((const Resolver*)resource)->config;xl6ExecutionResourceLeaveV1(&scope);return true;
}
bool xl6ExecutionNetResolverStatsV1(Resource* resource,xnetresolverstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=scope.resolver&&xrtNetResolverStats(scope.resolver,output);
    if(!scope.resolver)xrtSetErrorKind(XERR_STATE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
xfuture* xl6ExecutionNetResolverQueryV1(Resource* resource,const char* host,int family)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,true,false,&scope))return NULL;
    xfuture* result=xrtNetResolveAsync(scope.resolver,host,(xnetfamily)family);
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
