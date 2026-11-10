#include "services/semantic_color_result.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
static xatomic32 hold,entered,freed;static void* tracked;static DWORD owner;
static const xvalue* gatedArrayGet(const xvalue* v,size_t i)
{
    if(xrtAtomic32Load(&hold,XMEMORY_ACQUIRE) && GetCurrentThreadId()!=owner){
        xrtAtomic32Store(&entered,1,XMEMORY_RELEASE);
        for(int n=0;n<5000 && xrtAtomic32Load(&hold,XMEMORY_ACQUIRE);++n)xrtSleep(1);
    }
    return xrtValueArrayGet(v,i);
}
static void watchedFree(void* p)
{if(p && p==tracked)xrtAtomic32Store(&freed,GetCurrentThreadId()==owner?-1:1,XMEMORY_RELEASE);xrtFree(p);}
#define xrtValueArrayGet gatedArrayGet
#define xrtFree watchedFree
#include "services/semantic_color_result.c"
#undef xrtValueArrayGet
#undef xrtFree
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"semantic_color_worker_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xvalue* parse(const char* s){return xrtJsonParse(xrtStrView(s));}
static int waitFlag(xatomic32* flag)
{for(int i=0;i<5000;++i){if(xrtAtomic32Load(flag,XMEMORY_ACQUIRE))return 1;xrtSleep(1);}return 0;}
static int waitAnswer(xide_color_decoder* d,xide_color_completion* out)
{for(int i=0;i<5000;++i){if(xideColorDecoderTake(d,out))return 1;xrtSleep(1);}return 0;}
static int verify(void)
{
    owner=GetCurrentThreadId();xide_color_legend legend={.typeCount=1,.types={XIDE_COLOR_VARIABLE},.delta=1};
    xls_text* text=xlsTextCreate("alpha",5,4096,NULL);CHECK(text);xvalue* v=parse("{\"resultId\":\"one\",\"data\":[0,0,5,0,0]}");CHECK(v);enum xide_color_error error;
    xide_color_result *base=xideColorDecode(text,&legend,v,NULL,0,NULL,NULL,&error),*other=xideColorDecode(text,&legend,v,NULL,0,NULL,NULL,&error);CHECK(base && other);xrtValueRelease(v);
    tracked=base->ranges;xide_color_decoder* d=xideColorDecoderCreate(NULL,NULL);CHECK(d);xrtAtomic32Store(&hold,1,XMEMORY_RELEASE);
    xvalue* old=parse("{\"result\":{\"data\":[0,0,5,0,0]}}");CHECK(old && xideColorDecoderSubmit(d,1,old,xlsTextRetain(text),&legend,NULL,0));CHECK(waitFlag(&entered));
    /* Worker is blocked INSIDE actual word decoding, not after completion. */
    xideColorDecoderCancel(d);xide_color_completion out={0};CHECK(!xideColorDecoderTake(d,&out));
    xvalue *newest=parse("{\"result\":{\"data\":[0,0,5,0,0]}}"),*extra=parse("{\"result\":null}");CHECK(newest && extra);
    CHECK(xideColorDecoderSubmit(d,2,newest,xlsTextRetain(text),&legend,NULL,0));
    CHECK(!xideColorDecoderSubmit(d,3,extra,text,&legend,NULL,0));
    CHECK(xideColorDecoderRetire(d,base) && !xideColorDecoderRetire(d,other));CHECK(!xrtAtomic32Load(&freed,XMEMORY_ACQUIRE));
    xideColorResultRelease(other);xrtValueRelease(extra);xrtAtomic32Store(&hold,0,XMEMORY_RELEASE);
    CHECK(waitAnswer(d,&out) && out.token==2 && !out.error && out.result && out.result->count==1);
    CHECK(waitFlag(&freed) && xrtAtomic32Load(&freed,XMEMORY_ACQUIRE)==1);
    CHECK(xideColorDecoderRetire(d,out.result));
    /* Shutdown alone joins, and drains owned pending JSON/base/text as well. */
    extra=parse("{\"result\":null}");CHECK(extra && xideColorDecoderSubmit(d,3,extra,xlsTextRetain(text),&legend,NULL,0));xideColorDecoderDestroy(d);xlsTextRelease(text);tracked=NULL;return 1;
}
int main(void)
{if(!verify())return 1;printf("semantic_color_worker_test passed: %u checks; real in-decode barrier, nonblocking cancellation, latest-only publication, bounded retirement and shutdown.\n",checks);return 0;}
