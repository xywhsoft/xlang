#ifndef XL6_FIXTURE_XUI_CODE_SEARCH_COST_H
#define XL6_FIXTURE_XUI_CODE_SEARCH_COST_H
#include <xge3_runtime.h>
#include <stdio.h>

static xmemdebugsnapshot fixtureSearchBefore;

static int fixtureLocateBegin(void)
{
    if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 0;
    xrtMemDebugSnapshot(&fixtureSearchBefore);
    return 1;
}

static int fixtureLocateEnd(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return after.AllocCount==fixtureSearchBefore.AllocCount &&
           after.FreeCount==fixtureSearchBefore.FreeCount &&
           after.ReallocCount==fixtureSearchBefore.ReallocCount;
}

static int fixtureLocateNative(xui_code_document document)
{
    int checksum=0;
    for(int i=0;i<1000;++i){
        xui_code_range_t span;
        if(xuiCodeSearchLocatePlain(document,"needle",0,0,-1,0,&span)!=1) return -1;
        checksum+=span.iEnd;
    }
    return checksum;
}

static int fixtureLocateFail(void) { return xrtMemDebugFailAfter(0); }
static void fixtureLocateClear(void) { xrtMemDebugFailClear(); }
#endif
