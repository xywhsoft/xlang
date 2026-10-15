#define XRT_MODULE_MEMORY
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "xl6_ownership_catalog.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

struct XL6_OwnershipCatalog {
    const XL6_OwnershipDomainV1** domains;
    size_t count;
    bool building,failed;
    XL6_OwnershipPoliciesV1 policies;
#define XL6_POLICY_STORAGE(Name, Type) const struct Type** Name; size_t Name##Capacity;
    XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_STORAGE)
#undef XL6_POLICY_STORAGE
};
static void catalogPropagateFailure(xl6_error_result result)
{
    /* End restored the caller's diagnostic. Keep that earlier failure while
     * still delivering an explicit failure event to the enclosing scope;
     * its already-latched NULL failure cannot be replaced by this error. */
    xerror* previous=xrtTakeError();
    if(previous){xrtErrorFree(result.error);result.error=previous;}
    xl6ErrorScopePropagateFailure(result);
}

#define XL6_POLICY_APPEND(Name, Type) \
bool xl6OwnershipCatalogAdd##Name##V1(XL6_OwnershipCatalog* catalog,const struct Type* policy) \
{ \
    if(!catalog || !catalog->building || !policy){ \
        if(catalog && catalog->building)catalog->failed=true; \
        xrtSetErrorKind(XERR_ARGUMENT);return false; \
    } \
    size_t count=catalog->policies.Name##Count; \
    for(size_t i=0;i<count;++i)if(catalog->Name[i]==policy)return true; \
    if(count==catalog->Name##Capacity){ \
        size_t capacity=catalog->Name##Capacity; \
        if(capacity>SIZE_MAX/2 || (capacity ? capacity*2 : 4)>SIZE_MAX/sizeof(*catalog->Name)){ \
            catalog->failed=true;xrtSetErrorKind(XERR_RANGE);return false; \
        } \
        capacity=capacity ? capacity*2 : 4; \
        const struct Type** next=xrtRealloc(catalog->Name,capacity*sizeof(*catalog->Name)); \
        if(!next){catalog->failed=true;return false;} \
        catalog->Name=next;catalog->Name##Capacity=capacity;catalog->policies.Name=next; \
    } \
    catalog->Name[count]=policy;catalog->policies.Name##Count=count+1;return true; \
}
XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_APPEND)
#undef XL6_POLICY_APPEND

void xl6OwnershipCatalogDestroyV1(XL6_OwnershipCatalog* catalog)
{
    if(!catalog)return;
#define XL6_POLICY_FREE(Name, Type) xrtFree(catalog->Name);
    XL6_OWNERSHIP_POLICY_GROUPS(XL6_POLICY_FREE)
#undef XL6_POLICY_FREE
    xrtFree(catalog->domains);xrtFree(catalog);
}
XL6_OwnershipCatalog* xl6OwnershipCatalogCreateV1(
    const XL6_OwnershipDomainV1* const* domains,size_t count)
{
    if((count && !domains) || count>SIZE_MAX/sizeof(*domains)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    for(size_t i=0;i<count;++i){
        if(!domains[i] || domains[i]->size!=sizeof(*domains[i]) || (!domains[i]->Load && !domains[i]->Resolve && !domains[i]->AdmitsRead)){
            xrtSetErrorKind(XERR_ARGUMENT);return NULL;
        }
        for(size_t j=0;j<i;++j)if(domains[i]==domains[j]){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    }
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    XL6_OwnershipCatalog* catalog=xrtCalloc(1,sizeof(*catalog));
    if(!catalog)goto failed;
    if(count){
        catalog->domains=xrtMalloc(count*sizeof(*catalog->domains));
        if(!catalog->domains)goto failed;
        memcpy(catalog->domains,domains,count*sizeof(*catalog->domains));
    }
    catalog->count=count;catalog->building=true;
    for(size_t i=0;i<count;++i)
        if(domains[i]->Load && (!domains[i]->Load(catalog) || catalog->failed))goto failed;
    catalog->building=false;
    {
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        if(!outcome.ok){xl6OwnershipCatalogDestroyV1(catalog);catalogPropagateFailure(outcome);return NULL;}
        xrtErrorFree(outcome.error);return catalog;
    }
failed:
    xl6ErrorScopeCaptureFailure(&scope);
    xl6OwnershipCatalogDestroyV1(catalog);
    catalogPropagateFailure(xl6ErrorScopeEnd(&scope));return NULL;
}
bool xl6OwnershipCatalogResolveV1(xrtownershipref ref,void* data,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    const XL6_OwnershipCatalog* catalog=data;
    if(!adapter || !preparation || !completion){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *adapter=NULL;*preparation=NULL;*completion=NULL;
    if(!catalog || catalog->building){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    const xrtownershipadapterv1* selected=xrtValueOwnershipAdapterV1(ref);
    if(!selected)selected=xrtErrorOwnershipAdapterV1(ref);
    if(!selected)selected=xrtValueCursorOwnershipAdapterV1(ref);
    const xrtownershippreparationv1* pending=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;
    for(size_t i=0;i<catalog->policies.HandleCount;++i){
        const XL6_NativeHandlePolicyV1* policy=catalog->policies.Handle[i];
        if(policy->size!=sizeof(*policy) || policy->abiTag!=XL6_NATIVE_HANDLE_POLICY_V1_TAG || !policy->Ops || !policy->Trace){
            xrtSetErrorKind(XERR_STATE);return false;
        }
        const xrtownershipadapterv1* current=xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace);
        if(current){
            if(selected && selected!=current){xrtSetErrorKind(XERR_STATE);return false;}
            selected=current;
        }
    }
    for(size_t i=0;i<catalog->count;++i){
        if(!catalog->domains[i]->Resolve)continue;
        const xrtownershipadapterv1* current=NULL;
        const xrtownershippreparationv1* ready=NULL;
        const XL6_OwnershipFinishV1* step=NULL;
        bool known=catalog->domains[i]->Resolve(ref,&catalog->policies,&current,&ready,&step);
        if((!known && (current || ready || step)) || (known && (!current ||
            (ready && (ready->size!=sizeof(*ready) || ready->Adapter!=current)) ||
            (step && (step->size!=sizeof(*step) || step->Adapter!=current)))) ||
            (known && selected && (selected!=current || pending!=ready || finish!=step))){
            xrtSetErrorKind(XERR_STATE);return false;
        }
        if(known){selected=current;pending=ready;finish=step;}
    }
    if(!selected)return false;
    *adapter=selected;*preparation=pending;*completion=finish;return true;
}
bool xl6OwnershipCatalogAdmitsReadV1(xrtownershipref ref,void* data)
{
    const XL6_OwnershipCatalog* catalog=data;
    if(!catalog || catalog->building)return false;
    for(size_t i=0;i<catalog->count;++i)
        if(catalog->domains[i]->AdmitsRead && catalog->domains[i]->AdmitsRead(ref))return true;
    return false;
}
