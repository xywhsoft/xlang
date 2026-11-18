#include "foreign_xge_particle_renderer.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureParticleRenderBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void fixtureParticleRenderEnd(int mode)
{
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
    printf("PARTICLE_RENDER_HEAP mode=%d alloc=0 free=0 realloc=0\n", mode);
}

void fixtureParticleRenderNative(xge_particle_renderer renderer, xge_particle_world world)
{
    fixtureParticleRenderBegin();
    for (int i = 0; i < 100; ++i)
        assert(xgeParticleRender(renderer, world, NULL) == XGE_OK);
    fixtureParticleRenderEnd(1);
}
