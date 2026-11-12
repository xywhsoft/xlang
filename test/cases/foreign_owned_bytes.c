#include "foreign_owned_bytes.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void* produced;
static xmemdebugsnapshot start;
static uint64_t native_allocs,native_frees,native_reallocs;
int fixtureOwnedBytes(int mode,int fail_after,void** data,size_t* size)
{
    *data=NULL;*size=0;produced=NULL;
    if(mode==2)return 0; /* valid null empty */
    if(mode==3){*size=1;return 0;} /* invalid null nonempty */
    *data=xrtMalloc(4);if(!*data)return -9;
    memcpy(*data,"A\0\xffZ",4);produced=*data;*size=mode==4 ? 0:4;
    if(mode==5)*size=SIZE_MAX;
    if(fail_after>=0)(void)xrtMemDebugFailAfter(fail_after);
    if(mode==1)return -17; /* failure still owns an allocated output */
    if(mode==6){xrtSetErrorKind(XERR_ARGUMENT);return 0;}
    return 0;
}
int fixtureOwnedChars(char** data,int marker,uint64_t* size)
{
    void* value=NULL;size_t count=0;assert(marker==31);
    int status=fixtureOwnedBytes(0,-1,&value,&count);*data=value;*size=(uint64_t)count;return status ? status:17;
}
int fixtureOwnedUChars(unsigned char** data,size_t* size)
{
    void* value=NULL;int status=fixtureOwnedBytes(0,-1,&value,size);*data=value;return status;
}
int fixtureOwnedPrefix(const unsigned char* input,size_t count,void** data,size_t* size)
{
    *data=NULL;*size=0;if(!count)return 0;
    *data=xrtMalloc(count);if(!*data)return -9;
    memcpy(*data,input,count);*size=count;return 0;
}
int fixtureOwnedBytesIdentity(const unsigned char* data,size_t size)
{return data==produced && size==4 && !memcmp(data,"A\0\xffZ",4);}
void fixtureOwnedBytesReset(void){(void)xrtMemDebugFailAfter(-1);}
void fixtureOwnedBytesBegin(void){xrtMemDebugSnapshot(&start);}
static void counts(uint64_t* a,uint64_t* f,uint64_t* r)
{
    xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);
    *a=end.AllocCount-start.AllocCount;*f=end.FreeCount-start.FreeCount;*r=end.ReallocCount-start.ReallocCount;
}
void fixtureOwnedBytesEnd(void)
{uint64_t a,f,r;counts(&a,&f,&r);assert(a==f && r==0);}
void fixtureOwnedBytesNativeReference(void)
{
    int i;fixtureOwnedBytesBegin();
    for(i=0;i<100;++i){void* data=NULL;size_t size=0;
        assert(fixtureOwnedBytes(0,-1,&data,&size)==0);bytes owned=data;
        xvalue* value=xrtValueBytesTake(&owned,size);assert(value && !owned);
        xbytesview view={0};assert(xrtValueGetBytes(value,&view) && view.Data==data && view.Size==size);
        assert(fixtureOwnedBytesIdentity(view.Data,view.Size));
        xrtValueRelease(value);
    }
    counts(&native_allocs,&native_frees,&native_reallocs);assert(native_allocs==native_frees);
}
void fixtureOwnedBytesLanguageReference(void)
{
    uint64_t a,f,r;counts(&a,&f,&r);assert(a==native_allocs && f==native_frees && r==native_reallocs);
    printf("Owned bytes: native/language 100 takes: %llu allocations, %llu frees, %llu reallocations; identical payload pointers\n",
        (unsigned long long)a,(unsigned long long)f,(unsigned long long)r);
}
