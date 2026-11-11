#include "foreign_xge3_shape_matrix.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureMatrixHotBegin(void){xrtMemDebugSnapshot(&before);}
void fixtureMatrixHotEnd(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    puts("Shape matrix hot calls: zero XRT heap operations");
}
