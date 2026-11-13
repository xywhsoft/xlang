#include "foreign_owned_signed_output.h"
#include <xrt_decl.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
static void* produced;
static unsigned calls;
static xmemdebugsnapshot before;
static int produce(int mode,void** data,int* count)
{
    assert(data && count && !*data && !*count);
    ++calls;produced=NULL;
    if(mode==3){*count=-1;return 0;}
    if(mode==5)return 0;
    if(mode==9){*count=1;return 0;}
    *data=xrtMalloc(5);if(!*data)return -9;
    memcpy(*data,"A\0BZ",5);produced=*data;*count=4;
    if(mode==1 || mode==4)*count=-1;
    if(mode==2)*count=INT_MIN;
    if(mode==4)return -17;
    if(mode==6){*count=0;((char*)*data)[0]=0;}
    if(mode==7){assert(xrtMemDebugEnabled());assert(xrtMemDebugFailAfter(0));}
    if(mode==8)xrtSetErrorKind(XERR_ARGUMENT);
    return 0;
}
int signedOutputBytes(int mode,void** data,int* count)
{return produce(mode,data,count);}
int signedOutputText(int mode,char** data,int* count)
{
    void* raw=*data;int status=produce(mode,&raw,count);*data=raw;return status;
}
int signedOutputBytesIdentity(const unsigned char* data,size_t count)
{return data==produced && count==4 && !memcmp(data,"A\0BZ",4);}
int signedOutputTextIdentity(const char* data,size_t count)
{return signedOutputBytesIdentity((const unsigned char*)data,count);}
unsigned signedOutputCalls(void){return calls;}
void signedOutputReset(void){xrtMemDebugFailClear();}
void signedOutputBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}
void signedOutputEnd(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
}
