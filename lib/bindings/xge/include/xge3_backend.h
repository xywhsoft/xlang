#ifndef XL6_XGE3_BACKEND_H
#define XL6_XGE3_BACKEND_H
#include "xge3_checked_graphics.h"

/* Cold typed lifecycle policies only. Backend calls and frame methods in the
 * language declarations target the original XGE entries directly. */
static inline bool xl6Xge3HostSessionRequestClose(void* raw);
#define XL6_XGE_BACKEND_POLICY(Name,Type,Native,Label,RequestClose) \
static inline bool xl6Xge3##Name##Destroy(void* raw) \
{bool consumed=false;int status=Native##ReleaseOwnerChecked((Type)raw,&consumed); \
 return status==XGE_STATUS_DEFERRED ? consumed:xl6Xge3GraphicsConsumed(status,consumed,#Native "ReleaseOwnerChecked");} \
static inline bool xl6Xge3##Name##Trace(const void* raw,XL6_ModuleOwnershipVisit visitor,void* user) \
{return visitor(Native##Ownership((Type)raw),user);} \
static inline bool xl6Xge3##Name##Busy(const void* raw) \
{xrtownershipref ref=Native##Ownership((Type)raw); \
 const xrtownershipquiescencev1* quiet=Native##OwnershipQuiescenceV1(ref);return !quiet || quiet->Busy(ref.Data);} \
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3##Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Label, \
 0,1,NULL,NULL,xl6Xge3##Name##Destroy,0,1,NULL,NULL,xl6Xge3##Name##Trace,xl6Xge3##Name##Busy, \
 XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,RequestClose};return &value;} \
static inline bool xl6Xge3##Name##Resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies, \
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish) \
{(void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL; \
 const xrtownershipadapterv1* exact=Native##OwnershipAdapterV1(ref);if(!exact){return false;} \
 const xrtownershippreparationv1* ready=Native##OwnershipPreparationV1(ref); \
 const xrtownershipfinishv2* complete=Native##OwnershipCompletionV2(ref); \
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete || \
 complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG || \
 complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady){return false;} \
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;} \
static inline bool xl6Xge3##Name##Read(xrtownershipref ref){return Native##OwnershipReadReady(ref);} \
static inline bool xl6Xge3##Name##Finalization(xrtownershipref ref,const xrtownershipadapterv1* adapter, \
 const xrtownershipfinalizationv1** output) \
{*output=NULL;const xrtownershipadapterv1* exact=Native##OwnershipAdapterV1(ref);if(!exact){return true;} \
 if(exact!=adapter){return false;} \
 const xrtownershipfinalizationv1* value=Native##OwnershipFinalizationV1(ref); \
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG || \
 value->Adapter!=adapter || !value->CanStep || !value->Step){return false;} \
 *output=value;return true;} \
static inline const XL6_OwnershipDomainV1* xl6Xge3##Name##DomainV1Get(void) \
{static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3##Name##Resolve,xl6Xge3##Name##Read}, \
 XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3##Name##Finalization};return &value.V1;}

XL6_XGE_BACKEND_POLICY(PlatformBackend,xge_platform_backend_ref,xgePlatformBackend,"xge.backend.platform",NULL)
XL6_XGE_BACKEND_POLICY(GraphicsBackend,xge_graphics_backend_ref,xgeGraphicsBackend,"xge.backend.graphics",NULL)
XL6_XGE_BACKEND_POLICY(HostSession,xge_host_session,xgeHostSession,"xge.host.session",xl6Xge3HostSessionRequestClose)
#undef XL6_XGE_BACKEND_POLICY
/* Refuse before logical close: the caller must retain a usable endFrame()
 * method when a frame/pass/callback still owns its native obligation. */
static inline bool xl6Xge3HostSessionRequestClose(void* raw)
{
 if(!xl6Xge3HostSessionBusy(raw))return true;
 (void)xl6Xge3GraphicsConsumed(XGE_ERROR_BUSY,false,"xge.HostSession.close");return false;
}
#define xl6Xge3PlatformBackendPolicy (*xl6Xge3PlatformBackendPolicyGet())
#define xl6Xge3PlatformBackendDomainV1 (*xl6Xge3PlatformBackendDomainV1Get())
#define xl6Xge3GraphicsBackendPolicy (*xl6Xge3GraphicsBackendPolicyGet())
#define xl6Xge3GraphicsBackendDomainV1 (*xl6Xge3GraphicsBackendDomainV1Get())
#define xl6Xge3HostSessionPolicy (*xl6Xge3HostSessionPolicyGet())
#define xl6Xge3HostSessionDomainV1 (*xl6Xge3HostSessionDomainV1Get())
#endif
