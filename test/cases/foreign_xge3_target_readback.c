#define XRT_MODULE_ALL
#include "foreign_xge3_target_readback.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { width=67, height=5, row=width*4, stride=row+8, repeats=32 };
static xmemdebugsnapshot before;

void fixtureReadbackHeapStart(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureReadbackHeapReport(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("TARGET_READBACK_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

void fixtureReadbackBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(allowance));
}

void fixtureReadbackCheck(xge_render_target target)
{
    unsigned char padded[stride*height];
    memset(padded, 0xa5, sizeof(padded));
    assert(xgeRenderTargetReadPixelsSized(target, padded, sizeof(padded), stride) == XGE_OK);
    const unsigned char middle[] = {0,0,0,0}, bottom[] = {0,0,255,255};
    /* Blending quantizes premultiplied color before readback unpremultiplies it. */
    assert(padded[0]>=248 && !padded[1] && !padded[2] && padded[3]==128);
    assert(memcmp(padded+2*stride, middle, sizeof(middle)) == 0);
    assert(memcmp(padded+4*stride, bottom, sizeof(bottom)) == 0);
    for (int y=0; y<height; ++y)
        for (int j=row; j<stride; ++j) assert(padded[y*stride+j]==0xa5);

    void* out = padded;
    size_t length = 13;
    assert(xgeRenderTargetReadPixelsOwned(NULL,&out,&length)==XGE_ERROR_INVALID_ARGUMENT && !out && !length);
    fixtureReadbackHeapStart();
    fixtureReadbackBudget(0);
    out = padded;
    length = 13;
    assert(xgeRenderTargetReadPixelsOwned(target,&out,&length)==XGE_ERROR_OUT_OF_MEMORY && !out && !length);
    fixtureReadbackBudget(-1);
    xrtClearError();

    fixtureReadbackHeapStart();
    for (int i=0; i<repeats; ++i) {
        assert(xgeRenderTargetReadPixelsOwned(target,&out,&length)==XGE_OK && length==row*height);
        for (int y=0; y<height; ++y)
            assert(memcmp((unsigned char*)out+y*row,padded+y*stride,row)==0);
        xrtFree(out);
    }
    fixtureReadbackHeapReport(0);
}
