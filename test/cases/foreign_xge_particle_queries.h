#ifndef XL6_TEST_PARTICLE_QUERIES_H
#define XL6_TEST_PARTICLE_QUERIES_H

#include <xge3_particle_world.h>

void fixtureParticleQueriesBegin(void);
void fixtureParticleQueriesEnd(int mode);
void fixtureParticleQueriesNative(xge_particle_world world, xge_particle_effect effect);

#endif
