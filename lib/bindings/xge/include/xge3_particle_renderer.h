#ifndef XL6_XGE3_PARTICLE_RENDERER_H
#define XL6_XGE3_PARTICLE_RENDERER_H

#include "xge3_checked_graphics.h"

static inline bool xl6Xge3ParticleRendererDestroy(void* raw)
{
    bool consumed = false;
    int status = xgeParticleRendererReleaseOwnerChecked((xge_particle_renderer)raw, &consumed);
    return xl6Xge3GraphicsConsumed(status, consumed, "xgeParticleRendererReleaseOwnerChecked");
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xge3ParticleRendererPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 policy = {
        sizeof(policy), XL6_FOREIGN_VIEW_V1_TAG, "xge.particleRenderer", 0, 1,
        NULL, NULL, xl6Xge3ParticleRendererDestroy, 0, 1, NULL, NULL, NULL, NULL,
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE, xl6Xge3ParticleRendererDestroy, NULL
    };
    return &policy;
}

#define xl6Xge3ParticleRendererPolicy (*xl6Xge3ParticleRendererPolicyGet())

#endif
