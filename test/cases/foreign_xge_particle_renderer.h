#ifndef XL6_TEST_PARTICLE_RENDERER_H
#define XL6_TEST_PARTICLE_RENDERER_H

#include <xge3_particle_renderer.h>

void fixtureParticleRenderBegin(void);
void fixtureParticleRenderEnd(int mode);
void fixtureParticleRenderNative(xge_particle_renderer renderer, xge_particle_world world);

#endif
