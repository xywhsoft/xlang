#ifndef XL6_XGE3_GRAPHICS_CONTEXT_H
#define XL6_XGE3_GRAPHICS_CONTEXT_H
#include "xge3_checked_graphics.h"
static inline bool xl6Xge3GraphicsContextDestroy(void* raw)
{
 bool consumed=false;int status=xgeGraphicsContextReleaseOwnerChecked((xge_graphics_context)raw,&consumed);
 if(status==XGE_STATUS_DEFERRED)return consumed;
 return xl6Xge3GraphicsConsumed(status,consumed,"xgeGraphicsContextReleaseOwnerChecked");
}
static inline bool xl6Xge3GraphicsContextTrace(const void* raw,XL6_ModuleOwnershipVisit visitor,void* user)
{return visitor(xgeGraphicsContextOwnership((xge_graphics_context)raw),user);}
static inline bool xl6Xge3GraphicsContextBusy(const void* raw)
{
 xrtownershipref ref=xgeGraphicsContextOwnership((xge_graphics_context)raw);
 const xrtownershipquiescencev1* quiet=xgeGraphicsContextOwnershipQuiescenceV1(ref);
 return !quiet || quiet->Busy(ref.Data);
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3GraphicsContextPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.graphics.context",0,1,NULL,NULL,xl6Xge3GraphicsContextDestroy,0,1,NULL,NULL,
  xl6Xge3GraphicsContextTrace,xl6Xge3GraphicsContextBusy,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};
 return &value;
}
static inline bool xl6Xge3GraphicsContextResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
 (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
 const xrtownershipadapterv1* exact=xgeGraphicsContextOwnershipAdapterV1(ref);if(!exact)return false;
 const xrtownershippreparationv1* ready=xgeGraphicsContextOwnershipPreparationV1(ref);
 const xrtownershipfinishv2* complete=xgeGraphicsContextOwnershipCompletionV2(ref);
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
  complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
  complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static inline bool xl6Xge3GraphicsContextRead(xrtownershipref ref)
{return xgeGraphicsContextOwnershipReadReady(ref);}
static inline bool xl6Xge3GraphicsContextFinalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
 const xrtownershipfinalizationv1** output)
{
 *output=NULL;const xrtownershipadapterv1* exact=xgeGraphicsContextOwnershipAdapterV1(ref);
 if(!exact)return true;
 if(exact!=adapter)return false;
 const xrtownershipfinalizationv1* value=xgeGraphicsContextOwnershipFinalizationV1(ref);
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
  value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
 *output=value;return true;
}
static inline const XL6_OwnershipDomainV1* xl6Xge3GraphicsContextDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3GraphicsContextResolve,xl6Xge3GraphicsContextRead},
  XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3GraphicsContextFinalization};return &value.V1;
}
#define xl6Xge3GraphicsContextPolicy (*xl6Xge3GraphicsContextPolicyGet())
#define xl6Xge3GraphicsContextDomainV1 (*xl6Xge3GraphicsContextDomainV1Get())
#endif
