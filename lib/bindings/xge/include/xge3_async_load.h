#ifndef XL6_XGE3_ASYNC_LOAD_H
#define XL6_XGE3_ASYNC_LOAD_H
#include "xge3_checked_graphics.h"
static inline bool xl6Xge3AsyncLoadDestroy(void* raw)
{
 bool consumed=false;int status=xgeAsyncLoadReleaseOwnerChecked((xge_async_owned)raw,&consumed);
 if(status==XGE_STATUS_DEFERRED)return consumed;
 return xl6Xge3GraphicsConsumed(status,consumed,"xgeAsyncLoadReleaseOwnerChecked");
}
static inline bool xl6Xge3AsyncLoadTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xgeAsyncLoadOwnership((xge_async_owned)raw),user);}
static inline bool xl6Xge3AsyncLoadBusy(const void* raw)
{
 xrtownershipref ref=xgeAsyncLoadOwnership((xge_async_owned)raw);
 const xrtownershipquiescencev1* quiet=xgeAsyncLoadOwnershipQuiescenceV1(ref);
 return !quiet || quiet->Busy(ref.Data);
}
#define XL6_XGE_ASYNC_POLICY(Name,Id) \
static inline const XL6_ForeignViewDescriptorV1* Name##Get(void) \
{ \
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG, \
  Id,0,1,NULL,NULL,xl6Xge3AsyncLoadDestroy,0,1,NULL,NULL,xl6Xge3AsyncLoadTrace,xl6Xge3AsyncLoadBusy, \
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL}; \
 return &value; \
}
XL6_XGE_ASYNC_POLICY(xl6Xge3ImageLoadPolicy,"xge.load.image")
XL6_XGE_ASYNC_POLICY(xl6Xge3TextureLoadPolicy,"xge.load.texture")
#undef XL6_XGE_ASYNC_POLICY
static inline bool xl6Xge3AsyncLoadResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
 (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
 const xrtownershipadapterv1* exact=xgeAsyncLoadOwnershipAdapterV1(ref);if(!exact)return false;
 const xrtownershippreparationv1* ready=xgeAsyncLoadOwnershipPreparationV1(ref);
 const xrtownershipfinishv2* complete=xgeAsyncLoadOwnershipCompletionV2(ref);
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
  complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
  complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static inline bool xl6Xge3AsyncLoadRead(xrtownershipref ref)
{return xgeAsyncLoadOwnershipReadReady(ref);}
static inline bool xl6Xge3AsyncLoadFinalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
 const xrtownershipfinalizationv1** output)
{
 *output=NULL;const xrtownershipadapterv1* exact=xgeAsyncLoadOwnershipAdapterV1(ref);
 if(!exact)return true;
 if(exact!=adapter)return false;
 const xrtownershipfinalizationv1* value=xgeAsyncLoadOwnershipFinalizationV1(ref);
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
  value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
 *output=value;return true;
}
static inline const XL6_OwnershipDomainV1* xl6Xge3AsyncLoadDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3AsyncLoadResolve,xl6Xge3AsyncLoadRead},
  XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3AsyncLoadFinalization};return &value.V1;
}
#define xl6Xge3ImageLoadPolicy (*xl6Xge3ImageLoadPolicyGet())
#define xl6Xge3TextureLoadPolicy (*xl6Xge3TextureLoadPolicyGet())
#define xl6Xge3AsyncLoadDomainV1 (*xl6Xge3AsyncLoadDomainV1Get())
#endif
