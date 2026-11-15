#include "foreign_xui_code_margin.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureMarginBudget(int n)
{if(n<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureMarginBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureMarginReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("CodeMargin heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureMarginHot(xui_code_margin_model model)
{
    for(int i=0;i<1000;++i){
        xui_code_margin_state_t state={0};float width=0;
        assert(xuiCodeMarginModelSetWidth(model,10,20)==XUI_OK);
        assert(xuiCodeMarginModelGetState(model,0,&state)==XUI_OK&&state.iId==10&&state.fWidth==20);
        assert(xuiCodeMarginModelGetTotalWidth(model,&width)==XUI_OK&&width==20);
    }
}
