#include "foreign_xge3_image_io.h"
#include <xge3_runtime.h>
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

void fixtureImageNativeRelease(void)
{
    const unsigned char pixels[] = {64, 0, 0, 128, 0, 64, 0, 128};
    void* encoded = NULL;
    size_t count = 0;
    assert(xgeImageEncodePNGEx(1, 2, pixels, 4, XGE_IMAGE_PREMULTIPLIED, &encoded, &count) == XGE_OK);
    assert(count > 8 && ((const unsigned char*)encoded)[0] == 137);
    xgeMemoryFree(encoded);
    xgeMemoryFree(NULL);
}
