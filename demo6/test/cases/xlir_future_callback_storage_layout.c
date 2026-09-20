#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#define XRT_MODULE_STRING
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct Payload {
    int64_t scalar;
    char* string;
    xvalue* first;
    xfuture* source;
    XL6_Closure* closure;
    xerror* error;
    xvalue* last;
} Payload;
#define SLOT(KIND, FIELD) {KIND,offsetof(Payload,FIELD),sizeof(((Payload*)0)->FIELD)}
static const XL6_CaptureSlot slots[]={
    SLOT(XL6_CAPTURE_TRIVIAL,scalar),SLOT(XL6_CAPTURE_STRING,string),SLOT(XL6_CAPTURE_XVALUE,first),
    SLOT(XL6_CAPTURE_FUTURE,source),SLOT(XL6_CAPTURE_CLOSURE,closure),SLOT(XL6_CAPTURE_ERROR,error),
    SLOT(XL6_CAPTURE_XVALUE,last)
};
static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),7,slots};
static Payload* live;
static xvalue *firstObject,*lastObject;
static unsigned calls,drops,probes,rejections,roundDrops,visits;
static bool taskMode,probe,failCleanup;
/* Diagnostic lane only. The default hard gate always includes isolation and
 * remains failed until ordinary XRT/closure terminal release is phased. */
static bool checkIsolation=true;
static DWORD WINAPI inspect_while_finalizing(void* unused)
{
    xrtownershipscope freeze={0};DWORD began=GetTickCount();size_t n=876;(void)unused;
    while(!xrtOwnershipFreezeTryBegin(&freeze)){
        if((DWORD)(GetTickCount()-began)>=3000){
            fputs("Normal class finalization retained ownership mutation while waiting for another thread's freeze\n",stderr);
            assert(false);
        }
        SwitchToThread();
    }
    xrtownershipref ref=taskMode?xl6TaskRuntimeV1Get()->Ownership(live):xl6FutureCallbackRuntimeV1Get()->Ownership(live);
    assert(!ref.Ops->Count(ref.Data,&n) && n==876);
    assert(xrtOwnershipScopeEnd(&freeze));++probes;return 0;
}
static bool finalize(xvalue* value,void* unused)
{
    (void)unused;
    if(!roundDrops)assert(value==lastObject && live->last==NULL && live->first==firstObject);
    else assert(roundDrops==1 && value==firstObject && live->first==NULL && live->last==NULL);
    ++roundDrops;++drops;
    if(probe){HANDLE thread=CreateThread(NULL,0,inspect_while_finalizing,NULL,0,NULL);
        assert(thread && WaitForSingleObject(thread,5000)==WAIT_OBJECT_0 && CloseHandle(thread));}
    if(failCleanup)xrtSetErrorKind(roundDrops==1?XERR_RANGE:XERR_STATE);
    return true;
}
static void entry(void) {}
static void initialize(Payload* p,xfuture* source)
{
    *p=(Payload){0};live=p;roundDrops=0;
    p->scalar=180;p->string=xrtStrDup("layout");p->source=xrtFutureRef(source);
    p->first=xl6ClassRuntimeV2Get()->CommitTake(xl6ClassRuntimeV2Get()->Create(NULL,finalize));
    p->last=xl6ClassRuntimeV2Get()->CommitTake(xl6ClassRuntimeV2Get()->Create(NULL,finalize));
    p->closure=xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,NULL,NULL,NULL,NULL,0);
    p->error=xrtErrorCreate(XERR_ARGUMENT,"capture",180,"owned captured error");
    assert(p->string && p->source && p->first && p->last && p->closure && p->error);
    firstObject=p->first;lastObject=p->last;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;assert(!xrtGetError());xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void cleanup(Payload* p)
{
    if(taskMode)xl6TaskRuntimeV2Get()->Cleanup(p);else xl6FutureCallbackRuntimeV2Get()->Cleanup(p);
    assert(p->scalar==180 && !p->string && !p->first && !p->source && !p->closure && !p->error && !p->last);
}
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    Payload* p=data;xfuture* source=xrtFutureRef(p->source);assert(input && source);++calls;
    cleanup(p);cleanup(p);assert(roundDrops==2);
    if(xrtGetError()){assert(xrtErrorKind(xrtGetError())==XERR_RANGE && xrtPromiseReject(output,xrtGetError()));xrtClearError();}
    else if(xrtCancelRequested(cancel))assert(xrtPromiseCancel(output));
    else assert(xrtPromiseForward(output,source));
    xrtFutureDestroy(source);
}
static XL6_TaskOutcome run(xcancel* cancel,void* data,XL6_TaskResult* result)
{
    (void)cancel;++calls;cleanup(data);cleanup(data);*result=(XL6_TaskResult){0};
    assert(roundDrops==2);
    return xrtGetError()?XL6_TASK_FAILED:XL6_TASK_SUCCESS;
}
static bool visit(xrtownershipref ref,void* unused)
{ (void)unused;assert(ref.Data && ref.Ops);++visits;return true; }
static void refuse(const XL6_CaptureLayoutV1* bad,size_t size)
{
    unsigned before=calls;assert(!xl6CaptureRuntimeV1Get()->Validate(bad,size));
    assert(!xl6FutureCallbackRuntimeV2Get()->Allocate(NULL,size,callback,bad));xrtClearError();
    assert(!xl6TaskRuntimeV2Get()->Allocate(NULL,size,run,bad));xrtClearError();assert(before==calls);++rejections;
}
static void invalid_layouts(void)
{
    for(unsigned repeat=0;repeat<25;++repeat){
        xmemdebugsnapshot before;XL6_CaptureLayoutV1 bad;XL6_CaptureSlot copy[7];xrtMemDebugSnapshot(&before);
        refuse(NULL,sizeof(Payload));bad=layout;--bad.size;refuse(&bad,sizeof(Payload));
        bad=layout;--bad.bytes;refuse(&bad,sizeof(Payload));bad=layout;refuse(&bad,0);
        bad=layout;bad.slots=NULL;refuse(&bad,sizeof(Payload));bad=layout;bad.count=SIZE_MAX;refuse(&bad,sizeof(Payload));
        bad=layout;bad.count=0;refuse(&bad,sizeof(Payload));
        for(unsigned mode=0;mode<6;++mode){
            memcpy(copy,slots,sizeof(copy));bad=layout;bad.slots=copy;
            if(mode==0)copy[1].kind=0;
            if(mode==1)copy[1].kind=(XL6_CaptureKind)99;
            if(mode==2)copy[1].bytes=0;
            if(mode==3)--copy[1].bytes;
            if(mode==4)copy[1].offset=SIZE_MAX;
            if(mode==5)copy[1].offset=copy[0].offset;
            refuse(&bad,sizeof(Payload));
        }
        balanced(&before);
    }
}
static void future_paths(void)
{
    taskMode=false;
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;xfuture *source=NULL,*output;unsigned count=calls;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        Payload* p=xl6FutureCallbackRuntimeV2Get()->Allocate(NULL,sizeof(*p),callback,&layout);assert(p);initialize(p,source);
        probe=checkIsolation;failCleanup=n%5==4;output=xl6FutureCallbackRuntimeV1Get()->Start(p,source);assert(output && calls==count);
        xrtownershipscope freeze={0};xrtownershipresult graph;assert(xrtOwnershipFreezeTryBegin(&freeze));
        xrtownershipref root=xrtFutureOwnership(source);assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
        assert(graph.NodeCount>=10 && graph.ExternalRootCount==2);
        unsigned old=visits;assert(xl6CaptureRuntimeV1Get()->Trace(&layout,p,visit,NULL) && visits-old==5);
        assert(xrtOwnershipScopeEnd(&freeze));
        if(n%5==0)assert(xrtPromiseResolve(promise,NULL));
        else if(n%5==1){xrtSetErrorKind(XERR_ARGUMENT);assert(xrtPromiseReject(promise,xrtGetError()));xrtClearError();}
        else if(n%5==2)assert(xrtPromiseCancel(promise));else assert(xrtPromiseClose(promise));
        assert(calls==count+1 && roundDrops==2);
        assert(xrtFutureState(output)==(n%5==0?XFUTURE_RESOLVED:n%5==1||n%5==4?XFUTURE_FAILED:n%5==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);balanced(&before);
    }
}
static void task_and_rollback(void)
{
    for(unsigned kind=0;kind<2;++kind)for(unsigned n=0;n<50;++n){
        xmemdebugsnapshot before;xfuture* source=NULL;xrtMemDebugSnapshot(&before);taskMode=kind!=0;
        xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise && xrtPromiseResolve(promise,NULL));
        assert(xrtMemDebugFailAfter(0));
        void* failed=kind?xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(Payload),run,&layout):
            xl6FutureCallbackRuntimeV2Get()->Allocate(NULL,sizeof(Payload),callback,&layout);
        assert(!failed && xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();
        Payload* p=kind?xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(*p),run,&layout):
            xl6FutureCallbackRuntimeV2Get()->Allocate(NULL,sizeof(*p),callback,&layout);
        assert(p);initialize(p,source);probe=false;failCleanup=false;
        assert(xrtMemDebugFailAfter(0));
        if(kind)xl6TaskRuntimeV1Get()->Discard(p);else xl6FutureCallbackRuntimeV1Get()->Discard(p);
        assert(!xrtMemDebugFailTriggered() && roundDrops==2);xrtMemDebugFailClear();
        p=kind?xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(*p),run,&layout):
            xl6FutureCallbackRuntimeV2Get()->Allocate(NULL,sizeof(*p),callback,&layout);
        assert(p);initialize(p,source);probe=checkIsolation;failCleanup=false;unsigned prior=calls;
        if(kind){
            XL6_TaskPoolConfig config={1,0,0};XL6_TaskPool* pool=xl6TaskRuntimeV1Get()->PoolCreate(&config);assert(pool);
            xfuture* output=xl6TaskSubmitPayloadV1(pool,p,NULL,XL6_FUTURE_PAYLOAD_VALUE,false);
            assert(output && xl6TaskRuntimeV1Get()->PoolQuiesce(pool));
            assert(xrtFutureState(output)==XFUTURE_RESOLVED && xl6TaskRuntimeV1Get()->PoolDestroy(pool));xrtFutureDestroy(output);
        }else{
            xfuture* output=xl6FutureCallbackRuntimeV1Get()->Start(p,source);
            assert(output && xrtFutureState(output)==XFUTURE_RESOLVED);xrtFutureDestroy(output);
        }
        assert(calls==prior+1 && roundDrops==2);xrtFutureDestroy(source);xrtPromiseDestroy(promise);balanced(&before);
    }
}
int main(int argc,char** argv)
{
    assert(argc==1 || (argc==2 && !strcmp(argv[1],"--values-only")));
    checkIsolation=argc==1;
    xmemdebugsnapshot before;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    assert(xl6CaptureRuntimeV1Get()->Validate(&layout,sizeof(Payload)));
    invalid_layouts();future_paths();task_and_rollback();balanced(&before);
    assert(rejections==325 && calls==200 && drops==600 && probes==(checkIsolation?400u:0u) && visits==500);
    if(!checkIsolation){
        puts("Closed capture values-only diagnostic: 325 malformed layouts, 200 activations, 100 allocation failures, 100 allocation-free rollbacks, 600 ordered finalizers, zero live delta; DOES NOT PASS the cross-thread isolation hard gate");return 0;
    }
    puts("Closed capture layouts: 325 malformed layouts refused by both allocators, 100 pending and 50 ready Future callbacks, 50 accepted tasks, 100 allocation failures and 100 allocation-free rollbacks, 600 ordered finalizers, 400 cross-thread unfrozen active refusals; all six kinds and zero live delta");return 0;
}
