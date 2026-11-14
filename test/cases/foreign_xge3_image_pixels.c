#include "foreign_xge3_image_pixels.h"
#include <assert.h>
#include <stdio.h>
void fixturePixelBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
static xmemdebugsnapshot before;
void fixturePixelBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixturePixelReport(int label)
{
    static const char* labels[]={"language-copy","native-copy","language-snapshot","native-borrow"};
    assert(label>=0 && label<4);xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Image pixels heap %s: alloc=%llu free=%llu realloc=%llu\n",labels[label],
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixturePixelFormat(xge_image image,int unsupported)
{image->iFormat=unsupported?XGE_PIXEL_YUV420P:XGE_PIXEL_RGBA8;}
void fixturePixelNativeCopy(const xge_image_t* image,void* output,size_t size)
{for(int i=0;i<1000;++i)assert(xgeImageCopyPixels(image,output,size)==XGE_OK);}
void fixturePixelNativeBorrow(const xge_image_t* image)
{for(int i=0;i<1000;++i){xbytesview view;assert(xgeImageGetPixelView(image,&view)==XGE_OK && view.Size==16 && view.Data);}}
