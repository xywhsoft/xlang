#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct Payload {
    uint64_t number; xstrview string; xvalue* value; xfuture* future;
    XL6_Closure* closure; xerror* error; XL6_Cell* cell;
    XL6_ValueIterator* iterator; XL6_Generator* generator;
} Payload;
#define SLOT(K,F) {K,offsetof(Payload,F),sizeof(((Payload*)0)->F)}
static const XL6_CaptureSlot slots[]={SLOT(XL6_CAPTURE_TRIVIAL,number),SLOT(XL6_CAPTURE_STRING,string),
    SLOT(XL6_CAPTURE_XVALUE,value),SLOT(XL6_CAPTURE_FUTURE,future),SLOT(XL6_CAPTURE_CLOSURE,closure),
    SLOT(XL6_CAPTURE_ERROR,error),SLOT(XL6_CAPTURE_CELL,cell),SLOT(XL6_CAPTURE_VALUE_ITERATOR,iterator),SLOT(XL6_CAPTURE_GENERATOR,generator)};
static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),9,slots};
static unsigned drops,probes,edges,iteratorCases,rollbacks;
static const XL6_CaptureRuntimeV1* captures;
static const XL6_ValueIteratorRuntimeV1* iterators;
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static DWORD WINAPI freeze_probe(void* ignored)
{
    xrtownershipscope scope={0};(void)ignored;
    assert(xrtOwnershipFreezeTryBegin(&scope));assert(xrtOwnershipScopeEnd(&scope));return 0;
}
static void generator_drop(void* frame)
{
    HANDLE thread=CreateThread(NULL,0,freeze_probe,NULL,0,NULL);
    assert(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0&&CloseHandle(thread));++probes;++drops;
    xl6GeneratorRuntimeV1Get()->FrameFree(frame);xrtSetErrorKind(XERR_INTERNAL);
}
static bool generator_resume(XL6_Generator* generator)
{xl6GeneratorRuntimeV1Get()->Finish(generator);return false;}
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void closure_invoke(void){}
static XL6_TaskOutcome unused_run(xcancel* cancel,void* data,XL6_TaskResult* result)
{(void)cancel;(void)data;(void)result;abort();}
static bool edge(xrtownershipref ref,void* context)
{
    xrtownershipref* expected=context;size_t count=0;
    assert(edges<7&&ref.Data==expected[edges].Data&&ref.Ops==expected[edges].Ops);
    assert(ref.Ops->Count(ref.Data,&count)&&count);++edges;return true;
}
static void all_slots(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Payload* p=xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(*p),unused_run,&layout);assert(p);*p=(Payload){0};
    p->number=91;p->string=(xstrview){xrtStrDup("capture"),7};p->value=xrtValueInt(42);
    xpromise* promise=xrtPromiseCreate(&p->future,NULL);assert(promise);xrtPromiseDestroy(promise);
    p->closure=xl6ClosureRuntimeV1Get()->Create(NULL,closure_invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);
    p->error=xrtErrorCreate(XERR_STATE,"capture",7,"capture");
    p->cell=xl6CellRuntimeV1Get()->Create(XL6_CELL_I64,sizeof(int64_t));int64_t value=19;
    xl6CellRuntimeV1Get()->Initialize(p->cell,&value,sizeof(value));
    xvalue* source=xrtValueArray();assert(source&&xrtValueArrayAppendNew(source,xrtValueInt(81)));
    p->iterator=iterators->Create(source);xrtValueRelease(source);
    void* frame=xl6GeneratorRuntimeV1Get()->FrameAllocate(1);assert(frame);
    p->generator=xl6GeneratorRuntimeV1Get()->Create(NULL,frame,generator_resume,generator_drop,sizeof(int64_t),NULL,empty_trace,empty_trace);
    assert(p->string.Data&&p->value&&p->future&&p->closure&&p->error&&p->cell&&p->iterator&&p->generator);
    xrtownershipref expected[7];
    expected[0]=xrtValueOwnership(p->value);expected[1]=xrtFutureOwnership(p->future);
    expected[2]=xl6ClosureRuntimeV1Get()->Ownership(p->closure);expected[3]=xrtErrorOwnership(p->error);
    expected[4]=xl6CellRuntimeV1Get()->Ownership(p->cell);expected[5]=iterators->Ownership(p->iterator);
    expected[6]=xl6GeneratorRuntimeV1Get()->Ownership(p->generator);
    edges=0;assert(captures->Trace(&layout,p,edge,expected)&&edges==7);
    xrtSetErrorKind(XERR_STATE);xl6TaskRuntimeV2Get()->Cleanup(p);
    assert(xrtErrorKind(xrtGetError())==XERR_STATE&&p->number==91&&!p->string.Data&&!p->string.Size&&!p->value&&!p->future&&
        !p->closure&&!p->error&&!p->cell&&!p->iterator&&!p->generator);
    xl6TaskRuntimeV2Get()->Cleanup(p);assert(xrtErrorKind(xrtGetError())==XERR_STATE);
    xrtClearError();xl6TaskRuntimeV1Get()->Discard(p);balance(&before);
}
static bool cursor_edge(xrtownershipref ref,void* output)
{xrtownershipref* child=output;assert(!child->Data&&ref.Data&&ref.Ops);*child=ref;return true;}
static void iterator_case(unsigned kind)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* source=kind==0?xrtValueArray():kind==1?xrtValueIntMap():kind==2?xrtValueObject():xrtValueSet();assert(source);
    for(unsigned i=0;i<2;++i){xvalue* item=xrtValueInt(41+i);assert(item);
        if(!kind)assert(xrtValueArrayAppendNew(source,item));
        else if(kind==1)assert(xrtValueIntMapSetNew(source,10000000001LL+i,item));
        else if(kind==2)assert(xrtValueObjectSetNew(source,(xstrview){i?"b":"a",1},item));
        else assert(xrtValueSetAddNew(source,item));}
    XL6_ValueIterator* iterator=iterators->Create(source);assert(iterator);xrtValueRelease(source);
    xrtownershipref root=iterators->Ownership(iterator),child={0};size_t count=0;
    assert(root.Ops->Count(root.Data,&count)&&count==1&&root.Ops->Trace(root.Data,cursor_edge,&child));
    assert(xl6ValueIteratorOwnershipAdapterV1(root)&&!xl6ValueIteratorOwnershipAdapterV1(child));
    assert(xrtValueCursorOwnershipAdapterV1(child)&&!xrtValueCursorOwnershipAdapterV1(root));
    const XL6_ValueIteratorView* view=iterators->View(iterator);assert(view&&!view->item);
    unsigned mask=0,n=0;
    while(iterators->Advance(iterator)){
        view=iterators->View(iterator);int64_t value=0;assert(view&&view->item&&view->ordinal==n++);
        assert(xrtValueGetInt(view->item,&value)&&value>=41&&value<=42);mask|=1u<<(value-41);
        if(!kind)assert(view->key.Type==XVALUE_KEY_INDEX&&view->key.Index==n-1);
        else if(kind==1)assert(view->key.Type==XVALUE_KEY_INT&&view->key.Integer==10000000001LL+value-41);
        else if(kind==2)assert(view->key.Type==XVALUE_KEY_STRING&&view->key.String.Size==1&&*view->key.String.Data==(value==41?'a':'b'));
        else assert(view->key.Type==XVALUE_KEY_NONE);
        xrtownershipref next={0};assert(root.Ops->Trace(root.Data,cursor_edge,&next)&&next.Data==child.Data&&next.Ops==child.Ops);
    }
    assert(n==2&&mask==3&&!iterators->View(iterator)->item);
    const xrtownershipadapterv1* adapter=xl6ValueIteratorOwnershipAdapterV1(root);assert(adapter&&adapter->Hold(root.Data));
    assert(root.Ops->Count(root.Data,&count)&&count==2);iterators->Destroy(iterator);
    assert(root.Ops->Count(root.Data,&count)&&count==1);adapter->Drop(root.Data);++iteratorCases;balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));captures=xl6CaptureRuntimeV1Get();iterators=xl6ValueIteratorRuntimeV1Get();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    assert(captures->Validate(&layout,sizeof(Payload)));
    for(unsigned kind=1;kind<=9;++kind){
        size_t bytes=kind==XL6_CAPTURE_TRIVIAL?sizeof(uint64_t):
            kind==XL6_CAPTURE_STRING?sizeof(xstrview):sizeof(void*);
        XL6_CaptureSlot slot={kind,0,bytes};
        XL6_CaptureLayoutV1 one={sizeof(one),sizeof(Payload),1,&slot};assert(captures->Validate(&one,sizeof(Payload)));
        if(kind==XL6_CAPTURE_STRING){slot.bytes=sizeof(void*);assert(!captures->Validate(&one,sizeof(Payload)));slot.bytes=bytes;}
        slot.kind=(XL6_CaptureKind)99;assert(!captures->Validate(&one,sizeof(Payload)));}
    for(unsigned n=0;n<100;++n){all_slots();for(unsigned kind=0;kind<4;++kind)iterator_case(kind);}
    for(unsigned fail=0;fail<12;++fail){
        xvalue* source=xrtValueArray();assert(source&&xrtValueArrayAppendNew(source,xrtValueInt(51)));
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(fail));
        XL6_ValueIterator* iterator=iterators->Create(source);bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!iterator){assert(triggered&&xrtErrorKind(xrtGetError())==XERR_MEMORY);++rollbacks;}
        iterators->Destroy(iterator);balance(&before);xrtValueRelease(source);
    }
    assert(drops==100&&probes==100&&iteratorCases==400&&rollbacks);
    printf("Closed task captures: 9 physical kinds, 100 reverse cleanups and cross-thread freezes, 400 resident iterator snapshots, 12 OOM budgets; exact borrowed views, real pins and balanced allocations\n");return 0;
}
