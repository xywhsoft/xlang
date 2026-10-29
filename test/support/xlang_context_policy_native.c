#include "release/include/xruntime_decl.h"
#include "include/xl.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static XL_ContextOptions options;
static bool forbiddenTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{ (void)value;(void)visit;(void)context;assert(0);return false; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static xvalue* box(XL_Context* context)
{
    void* raw=context;xvalue* value=xl6NativeHandleTakeV1(&raw,xl6XlangContextHandlePolicyV1Get());assert(!raw);return value;
}
static void admission(XL_Context* context)
{
    xvalue* value=box(context);assert(value);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainXlangContextV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;const XL6_NativeHandlePolicyV1* policy=xl6XlangContextHandlePolicyV1Get();
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6OwnershipCatalogResolveV1(xrtValueOwnership(value),catalog,&adapter,&preparation,&completion));
    assert(adapter==xrtValueHandleOwnershipAdapterV1(xrtValueOwnership(value),policy->Ops,policy->Trace) && !preparation && !completion);
    assert(xrtOwnershipScopeEnd(&freeze));
    XL_Context* other=xlCreateContext(&options);assert(other);void* raw=other;
    xvaluehandleops copied=*policy->Ops;xvalue* foreign=xrtValueHandleTake(&raw,&copied,NULL);assert(foreign && !raw);
    assert(xrtValueHandleOwnershipBindPhased(foreign,forbiddenTrace));
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!xl6OwnershipCatalogResolveV1(xrtValueOwnership(foreign),catalog,&adapter,&preparation,&completion));
    assert(!adapter && !preparation && !completion && xrtOwnershipScopeEnd(&freeze));xrtValueRelease(foreign);
    xl6OwnershipCatalogDestroyV1(catalog);xrtValueRelease(value);
}
static xatomic32 entered={0},resume={0};
static void pauseCall(void)
{ xrtAtomic32Store(&entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&resume,XMEMORY_ACQUIRE))xrtThreadYield(); }
static int32 worker(void* data)
{ return xlRunScriptString(data,"extern \"context_policy_pause\" func pause()\npause()\n") ? 0 : 1; }
static void bufferPolicy(void)
{
    size_t point=0;
    for(;point<64;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xbuffer* buffer=xrtBufferCreate();assert(buffer && xrtBufferAppend(buffer,(xbytesview){(const uint8*)"a\0b",3}));
        void* owned=buffer;assert(xrtMemDebugFailAfter(point));
        xvalue* value=xl6NativeHandleTakeV1(&owned,xl6BufferHandlePolicyV1Get());
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(!owned && (hit ? !value : value!=NULL));
        if(value){
            const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainBufferV1Get()};
            XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
            xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
            assert(xl6OwnershipCatalogResolveV1(xrtValueOwnership(value),catalog,&adapter,&preparation,&completion) && adapter && !preparation && !completion);
            assert(buffer->Size==3 && buffer->Data[1]==0 && xrtOwnershipScopeEnd(&freeze));xl6OwnershipCatalogDestroyV1(catalog);
        }
        xrtValueRelease(value);balanced(&before);if(!hit)break;
    }
    assert(point<64);printf("buffer take prefixes=%zu first-no-hit=%zu balanced\n",point+1,point);
}
int main(int argc,char** argv)
{
    assert(argc==3 && xrtMemDebugEnable(true));
    options=xlDefaultContextOptions();options.tccDllPath=argv[1];options.tccLibPath=argv[2];
    XL_Context* anchor=xlCreateContext(&options);assert(anchor);
    int warmed=xlRunScriptString(anchor,"assert 40 + 2 == 42");
    if(!warmed)fprintf(stderr,"context warmup: %s\n",xlGetLastError(anchor));
    assert(warmed);
    XL_Context* warmup=xlCreateContext(&options);assert(warmup);admission(warmup);
    for(unsigned i=0;i<32;++i){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL_Context* context=xlCreateContext(&options);assert(context);
        admission(context);balanced(&before);
    }
    size_t point=0;
    for(;point<64;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL_Context* context=xlCreateContext(&options);assert(context);
        assert(xrtMemDebugFailAfter(point));xvalue* value=box(context);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert(hit ? !value : value!=NULL);xrtValueRelease(value);balanced(&before);if(!hit)break;
    }
    assert(point<64);printf("context take prefixes=%zu first-no-hit=%zu balanced\n",point+1,point);
    bufferPolicy();
    XL_Context* context=xlCreateContext(&options);assert(context && xlRegisterCFunction(context,"context_policy_pause",(void*)pauseCall));
    xvalue* value=box(context);assert(value);xthread* thread=xrtThreadCreate(worker,context,0);assert(thread);
    while(!xrtAtomic32Load(&entered,XMEMORY_ACQUIRE))xrtThreadYield();
    /* Drop requests destruction, while the real API active-call lease keeps
     * the native record alive. No fabricated owning language edge or unlock. */
    xrtValueRelease(value);xrtAtomic32Store(&resume,1,XMEMORY_RELEASE);
    assert(xrtThreadWait(thread)==XWAIT_OK && xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    xlDestroyContext(anchor);xrtClearError();puts("context policy: exact admission, owning take, real concurrent deferred destroy");return 0;
}
