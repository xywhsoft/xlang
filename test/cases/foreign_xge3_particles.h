#pragma once
#include <xge3_runtime.h>
void fixtureParticlesBudget(int);
void fixtureParticlesMark(void);
void fixtureParticlesBalanced(void);
void fixtureParticlesNativeFailures(const xge3d_particle_desc_t*);
void fixtureParticlesCompare(xge3d_particles*,const xge3d_particle_desc_t*);
void fixtureParticlesPair(xge3d_particles*,xge3d_particles*);
float fixtureParticlesInvalid(int);
void fixtureParticlesHeapBegin(void);
void fixtureParticlesHeapReport(int);
void fixtureParticlesNativeHot(xge3d_particles*);
void fixtureParticlesNativeCold(const xge3d_particle_desc_t*);
void fixtureParticlesColdReport(void);
int fixtureParticlesDuringCall(xge3d_particles*,void(*)(void*),void*);
void fixtureParticlesRetired(void);
