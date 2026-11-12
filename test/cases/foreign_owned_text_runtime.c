#include "src/runtime/xl6_c_output_text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool fail_diagnostic;
xerror* __real_xl6ErrorCreateV1(const char*,xstrview,int32_t,int32_t);
xerror* __wrap_xl6ErrorCreateV1(const char* kind,xstrview text,int32_t line,int32_t column)
{ return fail_diagnostic?NULL:__real_xl6ErrorCreateV1(kind,text,line,column); }
static char* payload(const char* text,size_t size)
{ char* result=xrtMalloc(size+1);assert(result);memcpy(result,text,size);result[size]=0;return result; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(now.LiveCount==before->LiveCount && now.LiveBytes==before->LiveBytes);
    assert(now.InvalidFreeCount==before->InvalidFreeCount && now.DoubleFreeCount==before->DoubleFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot baseline;xrtMemDebugSnapshot(&baseline);
    const char data[]={'a',0,(char)0xe4,(char)0xb8,(char)0xad};
    char* owned=payload(data,sizeof(data));xstrview output={0};
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xl6COutputTextTakeV1(owned,sizeof(data),true,7,9,&output));
    assert(output.Data==owned && output.Size==sizeof(data) && !memcmp(output.Data,data,sizeof(data)));
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.ReallocCount==after.ReallocCount && before.FreeCount==after.FreeCount);
    xrtFree((void*)output.Data);balanced(&baseline);
    assert(xl6COutputTextTakeV1(NULL,0,true,7,9,&output) && !output.Data && !output.Size);
    owned=payload("",0);assert(xl6COutputTextTakeV1(owned,0,true,7,9,&output) && output.Data==owned && !output.Size);xrtFree((void*)output.Data);
    for(unsigned i=0;i<2;++i) {
        const char bad[]={(char)0xff};owned=payload(bad,1);fail_diagnostic=i!=0;
        assert(!xl6COutputTextTakeV1(owned,1,true,7,9,&output) && !output.Data && !output.Size);
        if(fail_diagnostic)assert(!xrtGetError());else assert(!strcmp(xl6ErrorKindNameV1(xrtGetError()),"EncodingError"));
        xrtClearError();balanced(&baseline);
    }
    fail_diagnostic=false;
    owned=payload("a",1);owned[1]='X';
    assert(!xl6COutputTextTakeV1(owned,1,true,7,9,&output));xrtClearError();balanced(&baseline);
    owned=payload("",0);
    assert(!xl6COutputTextTakeV1(owned,UINT64_MAX,true,7,9,&output));xrtClearError();balanced(&baseline);
    assert(!xl6COutputTextTakeV1(NULL,1,true,7,9,&output));xrtClearError();balanced(&baseline);
    owned=payload("a",1);
    assert(!xl6COutputTextTakeV1(owned,1,false,7,9,&output) && !xrtGetError());balanced(&baseline);
    xerror* original=xl6ErrorCreateV1("ValueError",xrtStrView("primary native error"),2,3);assert(original);
    owned=payload("a",1);xrtSetErrorTake(original);
    assert(!xl6COutputTextTakeV1(owned,1,false,7,9,&output) && xrtGetError()==original);
    xrtClearError();balanced(&baseline);
    puts("Owned UTF-8 runtime: pointer identity, embedded NUL, zero success allocations, failure/OOM rollback passed");
    return 0;
}
