#ifndef XL6_XGE3_PARTICLE_WORLD_H
#define XL6_XGE3_PARTICLE_WORLD_H
#include "xge3_checked_graphics.h"
/* Cold lifecycle/graph metadata only. Particle world operations and exact typed
 * callback registrations call their native declarations directly. */
static inline bool xl6Xge3ParticleWorldDestroy(void* raw)
{
 bool consumed=false;int status=xgeParticleWorldReleaseOwnerChecked((xge_particle_world)raw,&consumed);
 if(status==XGE_STATUS_DEFERRED)return consumed;
 return xl6Xge3GraphicsConsumed(status,consumed,"xgeParticleWorldReleaseOwnerChecked");
}
static inline bool xl6Xge3ParticleWorldTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xgeParticleWorldOwnership((xge_particle_world)raw),user);}
static inline bool xl6Xge3ParticleWorldBusy(const void* raw)
{
 xrtownershipref ref=xgeParticleWorldOwnership((xge_particle_world)raw);
 const xrtownershipquiescencev1* quiet=xgeParticleWorldOwnershipQuiescenceV1(ref);
 return !quiet || quiet->Busy(ref.Data);
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3ParticleWorldPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.particleWorld",0,1,NULL,NULL,xl6Xge3ParticleWorldDestroy,0,1,NULL,NULL,xl6Xge3ParticleWorldTrace,xl6Xge3ParticleWorldBusy,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};
 return &value;
}
static inline bool xl6Xge3ParticleWorldResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
 const XL6_OwnershipFinishV1** finish)
{
 (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
 const xrtownershipadapterv1* exact=xgeParticleWorldOwnershipAdapterV1(ref);
 if(!exact)return false;
 const xrtownershippreparationv1* ready=xgeParticleWorldOwnershipPreparationV1(ref);
 const xrtownershipfinishv2* complete=xgeParticleWorldOwnershipCompletionV2(ref);
 if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
    complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
    complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
 *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static inline bool xl6Xge3ParticleWorldRead(xrtownershipref ref)
{return xgeParticleWorldOwnershipReadReady(ref);}
static inline bool xl6Xge3ParticleWorldFinalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
 const xrtownershipfinalizationv1** output)
{
 *output=NULL;const xrtownershipadapterv1* exact=xgeParticleWorldOwnershipAdapterV1(ref);
 if(!exact)return true;
 if(exact!=adapter)return false;
 const xrtownershipfinalizationv1* value=xgeParticleWorldOwnershipFinalizationV1(ref);
 if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
    value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
 *output=value;return true;
}
static inline const XL6_OwnershipDomainV1* xl6Xge3ParticleWorldDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xl6Xge3ParticleWorldResolve,xl6Xge3ParticleWorldRead},
  XL6_OWNERSHIP_DOMAIN_V2_TAG,xl6Xge3ParticleWorldFinalization};return &value.V1;
}
#define xl6Xge3ParticleWorldPolicy (*xl6Xge3ParticleWorldPolicyGet())
#define xl6Xge3ParticleWorldDomainV1 (*xl6Xge3ParticleWorldDomainV1Get())
#endif
