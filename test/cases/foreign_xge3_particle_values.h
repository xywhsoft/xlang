#ifndef XL6_TEST_PARTICLE_VALUES_H
#define XL6_TEST_PARTICLE_VALUES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureParticleHotBegin(void);
void fixtureParticleHotEnd(int native_reference);
double fixtureParticleNativeHot(const xge_particle_curve_t*,const xge_particle_gradient_t*);
int fixtureParticleEmitterSentinels(const xge_particle_emitter_t*);
#ifdef __cplusplus
}
#endif
#endif
