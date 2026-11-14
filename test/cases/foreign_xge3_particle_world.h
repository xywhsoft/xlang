#ifndef XL6_TEST_PARTICLE_WORLD_H
#define XL6_TEST_PARTICLE_WORLD_H
#include <xge3_particle_world.h>
void fixtureWorldBudget(int);
void fixtureWorldBegin(int);
void fixtureWorldEnd(void);
void fixtureWorldEqual(void);
void fixtureWorldHotBegin(void);
void fixtureWorldHotReport(int);
void fixtureWorldNativeHot(xge_particle_world);
void fixtureWorldNativeCollision(xge_particle_world,int);
void fixtureWorldNativeCollisionHot(xge_particle_world);
int fixtureWorldNativeCollisionCalls(void);
int fixtureWorldRefs(xge_particle_world);
#endif
