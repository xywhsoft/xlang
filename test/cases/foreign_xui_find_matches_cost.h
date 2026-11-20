#ifndef XL6_FIXTURE_XUI_FIND_MATCHES_COST_H
#define XL6_FIXTURE_XUI_FIND_MATCHES_COST_H
#include <xui3_find_matches.h>

static xmemdebugsnapshot fixtureFindBefore;

static int fixtureFindHeapBegin(void)
{
    if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true))return 0;
    xrtMemDebugSnapshot(&fixtureFindBefore);
    return 1;
}

static int fixtureFindHeapUnchanged(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return after.AllocCount==fixtureFindBefore.AllocCount &&
           after.ReallocCount==fixtureFindBefore.ReallocCount &&
           after.FreeCount==fixtureFindBefore.FreeCount;
}

static int fixtureFindNativeLoop(xui_find_matches_t* matches)
{
    const xui_find_result_t* items=NULL;
    size_t count=0;
    if(xuiFindMatchesGet(matches,&items,&count)!=XUI_OK || count!=3)return -1;
    int total=0;
    for(int pass=0;pass<1000;++pass)
        for(size_t i=0;i<count;++i)total+=items[i].arrCaptures[1].iEnd;
    return total;
}
#endif
