#include "foreign_xge3_font_xrf_bytes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
const char* fixtureXrfPath(void){return getenv("XLANG_XGE_FONT_FIXTURE");}
void fixtureXrfBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
static xmemdebugsnapshot before;
static uint64_t allocs,frees,reallocs;
void fixtureXrfBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
static void counts(uint64_t* a,uint64_t* f,uint64_t* r)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    *a=after.AllocCount-before.AllocCount;*f=after.FreeCount-before.FreeCount;*r=after.ReallocCount-before.ReallocCount;
}
void fixtureXrfEnd(void){uint64_t a,f,r;counts(&a,&f,&r);}
void fixtureXrfNativeReference(xge_font font)
{
    fixtureXrfBegin();
    for(int i=0;i<100;++i){
        void* data=NULL;int size=0;assert(xgeFontBuildXRFMemory(font,65,1,&data,&size)==XGE_OK && size>0);
        bytes owned=data;xvalue* value=xrtValueBytesTake(&owned,(size_t)size);assert(value && !owned);
        xbytesview view={0};assert(xrtValueGetBytes(value,&view) && view.Data==data && view.Size==(size_t)size);
        xrtValueRelease(value);
    }
    counts(&allocs,&frees,&reallocs);
}
void fixtureXrfLanguageReference(void)
{
    uint64_t a,f,r;counts(&a,&f,&r);assert(a==allocs && f==frees && r==reallocs);
    printf("XRF bytes heap: rounds=100 alloc=%llu free=%llu realloc=%llu; native/language equal\n",
        (unsigned long long)a,(unsigned long long)f,(unsigned long long)r);
}
