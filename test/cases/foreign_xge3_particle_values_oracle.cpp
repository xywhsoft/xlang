// A real independently compiled C++ caller/callee; no xlang/XRT implementation.
#include "foreign_xge3_particle_values_cpp.h"
xge_particle_curve_t fixtureParticleCurveCpp(xge_particle_curve_t value)
{value.arrKeys[7].fValue=123;return value;}
xge_particle_gradient_t fixtureParticleGradientCpp(xge_particle_gradient_t value)
{value.arrKeys[7].iColor=0x87654321u;return value;}
xge_particle_emitter_t fixtureParticleEmitterCpp(xge_particle_emitter_t value)
{++value.arrBursts[15].iCount;return value;}
xge_particle_event_t fixtureParticleEventCpp(xge_particle_event_t value)
{++value.iParticle;value.tPosition.fX=12;return value;}
