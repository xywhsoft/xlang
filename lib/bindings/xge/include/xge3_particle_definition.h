#ifndef XL6_XGE3_PARTICLE_DEFINITION_H
#define XL6_XGE3_PARTICLE_DEFINITION_H
#include <xge3_runtime.h>
/* A definition contains immutable native records, not callbacks or GPU state.
 * This root consumes exactly one native reference. World/renderer references
 * are independent; neither the outer pointer nor its emitter array is freed
 * separately. The ordinary resident creator-thread and active-call rules apply. */
static inline bool xl6Xge3ParticleDefinitionDestroy(void* raw)
{ xgeParticleDefinitionFree((xge_particle_definition)raw);return true; }
static inline const XL6_ForeignViewDescriptorV1* xl6Xge3ParticleDefinitionPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "xge.particleDefinition",0,1,NULL,NULL,xl6Xge3ParticleDefinitionDestroy,0,1,
  NULL,NULL,NULL,NULL,0,xl6Xge3ParticleDefinitionDestroy,NULL};
 return &policy;
}
#define xl6Xge3ParticleDefinitionPolicy (*xl6Xge3ParticleDefinitionPolicyGet())
#endif
