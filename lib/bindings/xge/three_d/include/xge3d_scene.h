#ifndef XL6_XGE3D_SCENE_H
#define XL6_XGE3D_SCENE_H
#include <xge3_checked_graphics.h>
#if !XGE3D_ENABLE_SCENE
#error "3D Scene hierarchy requires a matching XGE3D_ENABLE_SCENE native SDK"
#endif
typedef struct xl6_xge3d_scene_scope_t {xge3d_scene* scene;} xl6_xge3d_scene_scope_t;
typedef struct XL6_Xge3dSceneCallAlignment {char first;xl6_xge3d_scene_scope_t value;} XL6_Xge3dSceneCallAlignment;

/* Native access pins protect borrowed results until their language conversion
 * finishes. Only lifecycle/graph metadata lives here; business calls stay direct. */
static inline bool xl6Xge3dSceneBegin(void* raw,void* storage)
{
 int status=xge3dSceneAccessBegin((xge3d_scene*)raw);
 if(status!=XGE_OK)return xl6Xge3GraphicsConsumed(status,false,"xge3dSceneAccessBegin");
 ((xl6_xge3d_scene_scope_t*)storage)->scene=(xge3d_scene*)raw;return true;
}
static inline bool xl6Xge3dSceneEnd(void* storage)
{
 xl6_xge3d_scene_scope_t* scope=(xl6_xge3d_scene_scope_t*)storage;
 xge3d_scene* scene=scope->scene;scope->scene=NULL;
 int status=xge3dSceneAccessEnd(scene);
 return xl6Xge3GraphicsConsumed(status,status==XGE_OK,"xge3dSceneAccessEnd");
}
static inline bool xl6Xge3dSceneDestroy(void* raw)
{
 bool consumed=false;int status=xge3dSceneReleaseOwnerChecked((xge3d_scene*)raw,&consumed);
 if(status==XGE_STATUS_DEFERRED)return consumed;
 return xl6Xge3GraphicsConsumed(status,consumed,"xge3dSceneReleaseOwnerChecked");
}
static inline bool xl6Xge3dSceneTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xge3dSceneOwnership((xge3d_scene*)raw),user);}
static inline bool xl6Xge3dSceneBusy(const void* raw)
{
 xrtownershipref ref=xge3dSceneOwnership((xge3d_scene*)raw);
 const xrtownershipquiescencev1* quiet=xge3dSceneOwnershipQuiescenceV1(ref);
 return !quiet || quiet->Busy(ref.Data);
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3dScenePolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,
  "xge3d.scene",0,1,NULL,NULL,xl6Xge3dSceneDestroy,sizeof(xl6_xge3d_scene_scope_t),offsetof(XL6_Xge3dSceneCallAlignment,value),
  xl6Xge3dSceneBegin,xl6Xge3dSceneEnd,xl6Xge3dSceneTrace,xl6Xge3dSceneBusy,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,xl6Xge3dSceneDestroy,NULL};
 return &value;
}
static inline bool xl6Xge3dSceneResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
 const XL6_OwnershipFinishV1** finish)
{
 (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
 const xrtownershipadapterv1* exact=xge3dSceneOwnershipAdapterV1(ref);
 if(!exact)return false;
 const xrtownershippreparationv1* ready=xge3dSceneOwnershipPreparationV1(ref);
 const xrtownershipfinishv2* complete=xge3dSceneOwnershipCompletionV2(ref);
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
    complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
    complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static inline bool xl6Xge3dSceneRead(xrtownershipref ref)
{return xge3dSceneOwnershipReadReady(ref);}
static inline bool xl6Xge3dSceneFinalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
 const xrtownershipfinalizationv1** output)
{
 *output=NULL;const xrtownershipadapterv1* exact=xge3dSceneOwnershipAdapterV1(ref);
 if(!exact)return true;
 if(exact!=adapter)return false;
 const xrtownershipfinalizationv1* value=xge3dSceneOwnershipFinalizationV1(ref);
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
    value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
 *output=value;return true;
}
static inline const XL6_OwnershipDomainV1* xl6Xge3dSceneDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3dSceneResolve,xl6Xge3dSceneRead},
  XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3dSceneFinalization};return &value.V1;
}
#define xl6Xge3dScenePolicy (*xl6Xge3dScenePolicyGet())
#define xl6Xge3dSceneDomainV1 (*xl6Xge3dSceneDomainV1Get())
#endif
