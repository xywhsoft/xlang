#include "foreign_xge3_particle_world.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot prefix,hot;
void fixtureWorldBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureWorldBegin(int value)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);fixtureWorldBudget(value);}
void fixtureWorldEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureWorldEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(after.LiveCount!=prefix.LiveCount || after.LiveBytes!=prefix.LiveBytes)
  fprintf(stderr,"World live memory changed: count %zu -> %zu, bytes %zu -> %zu\n",prefix.LiveCount,after.LiveCount,prefix.LiveBytes,after.LiveBytes);
 assert(after.LiveCount==prefix.LiveCount && after.LiveBytes==prefix.LiveBytes && after.InvalidFreeCount==prefix.InvalidFreeCount && after.DoubleFreeCount==prefix.DoubleFreeCount);
}
void fixtureWorldHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureWorldHotReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("ParticleWorld heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureWorldNativeHot(xge_particle_world world)
{for(int i=0;i<1000;++i){assert(xgeParticleUpdate(world,0)==XGE_OK);assert(xgeParticleStats(world).iLiveParticles==0);}}
static size_t nativeRefs=1;static int nativeCalls;
static bool count(const void* raw,size_t* out){assert(raw==&nativeRefs);*out=nativeRefs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user){(void)raw;(void)visit;(void)user;return true;}
static const xrtownershipops nativeOps={count,trace};
static bool hold(const void* raw){assert(raw==&nativeRefs);++nativeRefs;return true;}
static bool drop(const void* raw){assert(raw==&nativeRefs && nativeRefs>1);--nativeRefs;return true;}
static int collision(xge_vec2_t from,xge_vec2_t to,float radius,xge_particle_hit_t* hit,void* user)
{(void)from;(void)radius;(void)user;assert(nativeRefs==3 && hit->fFraction==0);hit->tPosition=to;++nativeCalls;return 0;}
void fixtureWorldNativeCollision(xge_particle_world world,int install)
{
 xge_callback_owner_t owner={{&nativeRefs,&nativeOps},hold,drop};nativeCalls=0;
 assert(xgeParticleWorldSetCollisionOwned(world,install ? collision:NULL,NULL,install ? &owner:NULL)==XGE_OK);
 assert(nativeRefs==(install ? 2u:1u));
}
int fixtureWorldNativeCollisionCalls(void){return nativeCalls;}
void fixtureWorldNativeCollisionHot(xge_particle_world world)
{for(int i=0;i<1000;++i)assert(xgeParticleUpdate(world,1.0/64)==XGE_OK);}
int fixtureWorldRefs(xge_particle_world world)
{xrtownershipref ref=xgeParticleWorldOwnership(world);size_t refs=0;assert(ref.Ops && ref.Ops->Count(ref.Data,&refs));return (int)refs;}
