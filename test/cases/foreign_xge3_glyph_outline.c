#include "foreign_xge3_glyph_outline.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
const char* fixtureOutlinePath(void){return getenv("XLANG_XGE_FONT_FIXTURE");}
void fixtureOutlineBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
static xmemdebugsnapshot before;
void fixtureOutlineHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureOutlineHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Glyph outline heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureOutlineNativePerformance(xge_font font,xge_shape_ex shape,int glyph)
{
    for(int i=0;i<1000;++i){
        assert(xgeShapeExReset(shape)==XGE_OK);
        assert(xgeFontGlyphOutlineAppendShapeEx(font,glyph,shape,10,40)==XGE_OK);
    }
}
