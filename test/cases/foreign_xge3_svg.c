#include "foreign_xge3_svg.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
static xmemdebugsnapshot prefix;
void fixtureSvgHotBegin(void){xrtMemDebugSnapshot(&before);}
void fixtureSvgHotEnd(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    puts("SVG scalar/tuple hot calls: zero XRT heap operations");
}
void fixtureSvgPrefixBegin(int budget)
{
    xrtMemDebugSnapshot(&prefix);
    if(budget>=0)assert(xrtMemDebugFailAfter(budget));
}
void fixtureSvgPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureSvgHeapEqual(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(prefix.LiveCount==after.LiveCount && prefix.LiveBytes==after.LiveBytes &&
        prefix.InvalidFreeCount==after.InvalidFreeCount && prefix.DoubleFreeCount==after.DoubleFreeCount);
}
