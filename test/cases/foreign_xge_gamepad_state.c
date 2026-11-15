#include "foreign_xge_gamepad_state.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureGamepadHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureGamepadHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Gamepad heap %s: alloc=%llu free=%llu realloc=%llu; record=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount),sizeof(xge_gamepad_state_t));
}
void fixtureGamepadNativeHot(const xge_gamepad_state_t* state)
{
    for(int i=0;i<1000;++i){
        xge_gamepad_state_t copy={0};
        assert(xgeGamepadSetState(0,state)==XGE_OK&&xgeGamepadGetState(0,&copy)==XGE_OK);
        assert(copy.iButtons==state->iButtons&&copy.arrAxes[0]==state->arrAxes[0]);
    }
}
void fixtureGamepadBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
