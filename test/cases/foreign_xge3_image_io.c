#include "foreign_xge3_image_io.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
static xmemdebugsnapshot prefix;
void fixtureImageHotBegin(void){xrtMemDebugSnapshot(&before);}
void fixtureImageHotEnd(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    puts("Image metadata hot calls: zero XRT heap operations");
}
void fixtureImagePrefixBegin(int budget)
{
    xrtMemDebugSnapshot(&prefix);
    if(budget>=0)assert(xrtMemDebugFailAfter(budget));
}
void fixtureImagePrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureImageHeapEqual(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(prefix.LiveCount==after.LiveCount && prefix.LiveBytes==after.LiveBytes &&
        prefix.InvalidFreeCount==after.InvalidFreeCount && prefix.DoubleFreeCount==after.DoubleFreeCount);
}
