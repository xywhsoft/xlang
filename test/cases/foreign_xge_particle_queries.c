#include "foreign_xge_particle_queries.h"

#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureParticleQueriesBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void fixtureParticleQueriesEnd(int mode)
{
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
    printf("PARTICLE_QUERY_HEAP mode=%d alloc=0 free=0 realloc=0\n", mode);
}

void fixtureParticleQueriesNative(xge_particle_world world, xge_particle_effect effect)
{
    const xge_vec2_t start = {0, 2}, finish = {0, -2}, normal = {0, 2};
    const xge_rect_t rect = {0, 0, 8, 8};
    const xge_vec2_t left = {-4, 2}, right = {4, 2};
    fixtureParticleQueriesBegin();
    for (int i = 0; i < 1000; ++i) {
        bool result;
        xge_particle_event_t event;
        xge_particle_hit_t hit;
        assert(xgeParticleState(world, effect) == XGE_PARTICLE_PLAYING);
        assert(xgeParticleWorldPollEvent(world, &result, &event) == XGE_OK && !result);
        assert(xgeParticleSweepPlane(start, finish, .5f, normal, 0, &result, &hit) == XGE_OK);
        assert(result && hit.fFraction == .375f);
        assert(xgeParticleSweepRect(left, right, .5f, rect, &result, &hit) == XGE_OK);
        assert(result && hit.fFraction == .4375f);
    }
    fixtureParticleQueriesEnd(1);
}
