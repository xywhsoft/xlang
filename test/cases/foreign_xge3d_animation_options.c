#include "foreign_xge3d_animation_options.h"

#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureAnimationOptionsBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureAnimationOptionsEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("Animation options heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

void fixtureAnimationOptionsNative(xge3d_animator* animator,xge3d_clip* clip)
{
    xge3d_animation_layer_t legacy=xge3dAnimationLayerDefault();
    xge3d_animation_options_t options={0};
    xge3dAnimationOptionsDefaultInto(&options);
    assert(options.time==legacy.time && options.speed==legacy.speed);
    assert(options.weight==legacy.weight && options.loop==legacy.loop);
    assert(!legacy.mask && !legacy.mask_count && !legacy.bindings && !legacy.binding_count);
    assert(xge3dAnimatorPlayConfigured(animator,0,clip,NULL,0,NULL,0,NULL,0)==XGE_ERROR_INVALID_ARGUMENT);
    assert(xge3dAnimatorPlayConfigured(animator,0,clip,&options,0,NULL,1,NULL,0)==XGE_ERROR_INVALID_ARGUMENT);
    assert(xge3dAnimatorPlayArrays(animator,0,clip,NULL,0,NULL,0,NULL,0)==XGE_OK);
    float mask[]={1,1,1,.5f};
    xge3d_bone_binding_t map={1,3};
    fixtureAnimationOptionsBegin();
    for (int i=0;i<1000;++i)
        assert(xge3dAnimatorPlayConfigured(animator,0,clip,&options,0,mask,4,&map,1)==XGE_OK);
    fixtureAnimationOptionsEnd(1);
}

void fixtureAnimationOptionsFail(int prefix)
{
    if (prefix<0) xrtMemDebugFailClear();
    else {
        assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
        assert(xrtMemDebugFailAfter(prefix));
    }
}
