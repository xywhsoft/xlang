#include "foreign_xge3_image_alpha.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int fixtureAlphaPixels(xge_image image,int converted)
{
    static const unsigned char original[16]={200,100,50,128,255,128,64,0,255,128,64,1,9,17,31,255};
    static const unsigned char expected[16]={100,50,25,128,0,0,0,0,1,1,0,1,9,17,31,255};
    if(!image || image->iWidth!=2 || image->iHeight!=2 || image->iStride!=8)return 0;
    return !memcmp(image->pPixels,converted?expected:original,16);
}
void fixtureAlphaInvalid(xge_image image,int invalid)
{image->iStride=invalid?1:8;}
void fixtureAlphaBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
static xmemdebugsnapshot before;
void fixtureAlphaHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureAlphaHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Image alpha heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureAlphaNativePerformance(xge_image image)
{for(int i=0;i<1000;++i)assert(xgeImagePremultiplyChecked(image)==XGE_OK);}
