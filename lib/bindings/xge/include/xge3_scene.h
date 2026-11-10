#ifndef XL6_XGE3_SCENE_H
#define XL6_XGE3_SCENE_H
#include "xge3_checked_graphics.h"
/* Cold lifecycle/graph metadata only. Scene operations and exact typed
 * callback registrations call their native declarations directly. */
static inline bool xl6Xge3SceneDestroy(void* raw)
{
 bool consumed=false;int status=xgeSceneReleaseOwnerChecked((xge_scene_owned)raw,&consumed);
 if(status==XGE_STATUS_DEFERRED)return consumed;
 return xl6Xge3GraphicsConsumed(status,consumed,"xgeSceneReleaseOwnerChecked");
}
static inline bool xl6Xge3SceneTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xgeSceneOwnership((xge_scene_owned)raw),user);}
static inline bool xl6Xge3SceneBusy(const void* raw)
{
 xrtownershipref ref=xgeSceneOwnership((xge_scene_owned)raw);
 const xrtownershipquiescencev1* quiet=xgeSceneOwnershipQuiescenceV1(ref);
 return !quiet || quiet->Busy(ref.Data);
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3ScenePolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.scene",0,1,NULL,NULL,xl6Xge3SceneDestroy,0,1,NULL,NULL,xl6Xge3SceneTrace,xl6Xge3SceneBusy,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};
 return &value;
}
static inline bool xl6Xge3SceneResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
 const XL6_OwnershipFinishV1** finish)
{
 (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
 const xrtownershipadapterv1* exact=xgeSceneOwnershipAdapterV1(ref);
 if(!exact)return false;
 const xrtownershippreparationv1* ready=xgeSceneOwnershipPreparationV1(ref);
 const xrtownershipfinishv2* complete=xgeSceneOwnershipCompletionV2(ref);
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
    complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
    complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static inline bool xl6Xge3SceneRead(xrtownershipref ref)
{return xgeSceneOwnershipReadReady(ref);}
static inline bool xl6Xge3SceneFinalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
 const xrtownershipfinalizationv1** output)
{
 *output=NULL;const xrtownershipadapterv1* exact=xgeSceneOwnershipAdapterV1(ref);
 if(!exact)return true;
 if(exact!=adapter)return false;
 const xrtownershipfinalizationv1* value=xgeSceneOwnershipFinalizationV1(ref);
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
    value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
 *output=value;return true;
}
static inline const XL6_OwnershipDomainV1* xl6Xge3SceneDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3SceneResolve,xl6Xge3SceneRead},
  XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3SceneFinalization};return &value.V1;
}
#define xl6Xge3ScenePolicy (*xl6Xge3ScenePolicyGet())
#define xl6Xge3SceneDomainV1 (*xl6Xge3SceneDomainV1Get())
#endif
