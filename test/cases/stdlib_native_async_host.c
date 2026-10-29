#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct Frame {
    XL6_SlotMapKeyV1 key;unsigned char live;unsigned mode;
    XL6_ContinuationTicket ticket;xfuturewatch watch;xfuture* source;xpromise* outer;
} Frame;
static const XL6_ContinuationFrameRuntimeV5* api;
static const XL6_ActivationCaptureSlotV2 slots[]={
    {{{XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Frame,key),sizeof(XL6_SlotMapKeyV1)},xl6SlotMapKeyCaptureProfileV1Get},offsetof(Frame,live)},
    {{{XL6_CAPTURE_TRIVIAL,offsetof(Frame,mode),sizeof(unsigned)},NULL},SIZE_MAX}};
static const size_t cleanupOrder[]={0};
static const XL6_ActivationCaptureLayoutV2 captures={sizeof(captures),sizeof(Frame),2,slots,1,cleanupOrder};
static const XL6_ContinuationFrameLayoutV3 layout={sizeof(layout),&captures,
    offsetof(Frame,ticket),offsetof(Frame,watch),offsetof(Frame,source),offsetof(Frame,outer)};
static unsigned runs,vectors,refusals;
static bool visit(xrtownershipref ref,void* context){(void)context;assert(ref.Ops);return true;}
static Frame* create(unsigned mode)
{
    Frame* f=api->Allocate(sizeof(*f),&layout);if(!f)return NULL;
    f->key=(XL6_SlotMapKeyV1){0};f->live=0;f->mode=mode;
    f->key=(XL6_SlotMapKeyV1){xl6SlotMapInstanceCreateV1(),UINT64_C(0x100000001)};
    if(!f->key.instance){api->Discard(f);return NULL;}f->live=1;return f;
}
static void run(void* data)
{
    Frame* f=data;xfutureresult input={0};assert(xl6ContinuationFrameRuntimeV4Get()->Observe(f,&input));
    assert(f->live && f->key.instance);unsigned mode=f->mode;++runs;
    assert(xrtMemDebugFailAfter(0));
    xl6_error_result r=api->Cleanup(f,(xl6_error_result){mode!=1,NULL});
    bool allocated=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(!allocated && r.ok==(mode!=1) && !r.error && !f->live && !f->key.instance && !f->key.slot);
    assert(xrtPromiseClose(f->outer));
    if(!r.ok)xl6ErrorScopePropagateFailure(r);
}
static void scenario(bool ready,unsigned mode)
{
    XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(1);assert(queue);
    xfuture* source=NULL,*output=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
    if(ready)assert(xrtPromiseClose(promise));
    Frame* f=create(mode);assert(f);f->source=xrtFutureRef(source);f->outer=xrtPromiseCreate(&output,NULL);assert(f->source && f->outer);
    assert(xl6ContinuationReserve(queue,&f->ticket,run,api->Retire,f)==XL6_CONTINUATION_ADMITTED);
    assert(xl6ContinuationFrameRuntimeV3Get()->Bind(f));
    assert(xl6ContinuationWatchRegister(&f->ticket,&f->watch,f->source));
    xl6ContinuationCommit(&f->ticket);assert(api->Retain(f)==f);
    if(!ready && mode!=2)assert(xrtPromiseClose(promise));
    if(!ready && mode==2){assert(xl6ContinuationFrameRuntimeV4Get()->CancelWait(f));assert(!xl6ContinuationFrameRuntimeV4Get()->CancelWait(f));}
    xrtownershipref ref=api->Ownership(f);size_t count=0;assert(ref.Ops->Count(ref.Data,&count));
    assert(ref.Ops->Trace(ref.Data,visit,NULL));const xrtownershippreparationv1* preparation=NULL;
    assert(xl6ContinuationFrameOwnershipAdapterV1(ref,&preparation) && preparation);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(xl6ContinuationRunOne(queue) && !xl6ContinuationRunOne(queue));xl6_error_result r=xl6ErrorScopeEnd(&scope);
    assert(r.ok==(mode!=1) && !r.error && !f->live && !f->key.instance && !f->key.slot);
    assert(ref.Ops->Count(ref.Data,&count) && count==1);assert(ref.Ops->Trace(ref.Data,visit,NULL));
    api->Release(f);xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);
    xl6ContinuationQueueClose(queue);assert(xl6ContinuationQueueDestroy(queue));++vectors;
}
static void invalid(void)
{
    XL6_ContinuationFrameLayoutV3 bad=layout;
#define BAD(F,V) do{bad.F=(V);assert(!api->Allocate(sizeof(Frame),&bad));xrtClearError();bad=layout;++refusals;}while(0)
    BAD(size,0);BAD(captures,NULL);BAD(ticket,1);BAD(watch,offsetof(Frame,key));
    BAD(source,offsetof(Frame,key));BAD(outer,offsetof(Frame,source));BAD(ticket,SIZE_MAX);
#undef BAD
    XL6_ActivationCaptureLayoutV2 c=captures;c.bytes++;bad.captures=&c;
    assert(!api->Allocate(sizeof(Frame),&bad));xrtClearError();++refusals;
}
static void private_discard(void)
{
    Frame* f=create(0);assert(f);assert(api->Retain(f)==f);
    assert(xrtMemDebugFailAfter(0));api->Discard(f);
    bool allocated=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(!allocated && !f->live && !f->key.instance && !f->key.slot);api->Release(f);++vectors;
}
int main(int argc,char** argv)
{
    api=xl6ContinuationFrameRuntimeV5Get();xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==2){
        size_t at=(size_t)strtoull(argv[1],NULL,10);assert(xrtMemDebugFailAfter(at));Frame* f=create(0);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert((f!=NULL)==!hit);
        if(f)api->Discard(f);
        xrtClearError();scenario(false,0);printf("prefix=%zu hit=%u\n",at,(unsigned)hit);
    }else{
        assert(argc==1);for(unsigned i=0;i<200;++i){scenario(false,0);scenario(true,0);scenario(false,1);scenario(false,2);private_discard();}invalid();
    }
    assert(!xrtGetError() && !xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    printf("native async vectors=%u refusals=%u runs=%u; balanced\n",vectors,refusals,runs);return 0;
}
