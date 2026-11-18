#ifndef XL6_XGE3D_PARTICLES_H
#define XL6_XGE3D_PARTICLES_H
#include <xge3_render_resources.h>
#if !XGE3D_ENABLE_PARTICLES
#error "3D Particles requires a matching XGE3D_ENABLE_PARTICLES native SDK"
#endif
/* The native object owns only a numeric descriptor and its CPU storage.
 * Generic foreign-call scopes defer this one native free during active calls.
 * There are no callback, GPU, texture or other language ownership edges. */
XL6_XGE_NATIVE_POLICY(xl6Xge3dParticlesPolicy,"xge3d.particles",xge3d_particles,xge3dParticlesFree)
XL6_XGE_NATIVE_POLICY(xl6Xge3dBillboardBatchPolicy,"xge3d.billboardBatch",xge3d_billboard_batch,xge3dBillboardBatchFree)
#define xl6Xge3dParticlesPolicy (*xl6Xge3dParticlesPolicyGet())
#define xl6Xge3dBillboardBatchPolicy (*xl6Xge3dBillboardBatchPolicyGet())
#endif
