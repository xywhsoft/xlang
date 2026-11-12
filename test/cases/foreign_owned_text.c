#include "foreign_owned_text.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char payload[]={65,0,(char)0xe4,(char)0xbd,(char)0xa0,90};
static void* produced;
static xmemdebugsnapshot before;
static uint64_t allocs,frees,reallocs;
int fixtureOwnedText(int mode,char** data,uint64_t* size)
{
    *data=NULL;*size=0;produced=NULL;
    if(mode==1)return 0;
    if(mode==5){*size=1;return 0;}
    *data=xrtMalloc(sizeof(payload)+1);if(!*data)return -9;
    memcpy(*data,payload,sizeof(payload));(*data)[sizeof(payload)]=0;
    produced=*data;*size=sizeof(payload);
    if(mode==2)(*data)[2]=(char)0xff;
    if(mode==3)(*data)[sizeof(payload)]=1;
    if(mode==4)return -17;
    if(mode==6)*size=UINT64_MAX;
    if(mode==7)*size=0,(*data)[0]=0;
    if(mode==8)xrtSetErrorKind(XERR_ARGUMENT);
    if(mode==9)(void)xrtMemDebugFailAfter(0);
    return 0;
}
int fixtureOwnedTextIdentity(const char* data,size_t size)
{return data==produced && size==sizeof(payload) && !memcmp(data,payload,size);}
int fixtureOwnedTextCString(const char* data){return data && !*data;}
int fixtureOwnedTextFromSpan(const fixture_owned_options* options,const char* data,uint64_t count,char** output,uint64_t* size)
{
    assert(options && options->marker==31);*output=NULL;*size=0;
    if(count>=SIZE_MAX)return -9;
    *output=xrtMalloc((size_t)count+1);if(!*output)return -9;
    if(count)memcpy(*output,data,(size_t)count);
    (*output)[count]=0;*size=count;return 0;
}
void fixtureOwnedTextReset(void){(void)xrtMemDebugFailAfter(-1);}
void fixtureOwnedTextBegin(void){xrtMemDebugSnapshot(&before);}
static void counts(uint64_t* a,uint64_t* f,uint64_t* r)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    *a=after.AllocCount-before.AllocCount;*f=after.FreeCount-before.FreeCount;*r=after.ReallocCount-before.ReallocCount;
}
void fixtureOwnedTextEnd(void){uint64_t a,f,r;counts(&a,&f,&r);}
void fixtureOwnedTextNativeReference(void)
{
    fixtureOwnedTextBegin();
    for(int i=0;i<100;++i){char* p=NULL;uint64_t n=0;assert(fixtureOwnedText(0,&p,&n)==0);
        assert(fixtureOwnedTextIdentity(p,(size_t)n));assert(xrtUtf8Count((xstrview){p,(size_t)n})!=XRT_NPOS);xrtFree(p);}
    counts(&allocs,&frees,&reallocs);
}
void fixtureOwnedTextLanguageReference(void)
{
    uint64_t a,f,r;counts(&a,&f,&r);assert(a==allocs && f==frees && r==reallocs);
    printf("Owned UTF-8: native/language 100 takes: %llu allocations, %llu frees, %llu reallocations; exact payload pointers\n",
        (unsigned long long)a,(unsigned long long)f,(unsigned long long)r);
}
