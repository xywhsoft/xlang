#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_capture.h"
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
    char* string; xvalue* value; xfuture* future; XL6_Closure* closure;
    xerror* error; XL6_Cell* cell; XL6_ValueIterator* iterator; XL6_Generator* generator;
    void* loan; uint64_t number; unsigned char initialized[8];
} Payload;
#define SLOT(K,F,I) {{K,offsetof(Payload,F),sizeof(((Payload*)0)->F)},offsetof(Payload,initialized)+I}
static const XL6_ActivationCaptureSlot slots[]={
    SLOT(XL6_CAPTURE_STRING,string,0),SLOT(XL6_CAPTURE_XVALUE,value,1),SLOT(XL6_CAPTURE_FUTURE,future,2),
    SLOT(XL6_CAPTURE_CLOSURE,closure,3),SLOT(XL6_CAPTURE_ERROR,error,4),SLOT(XL6_CAPTURE_CELL,cell,5),
    SLOT(XL6_CAPTURE_VALUE_ITERATOR,iterator,6),SLOT(XL6_CAPTURE_GENERATOR,generator,7),
    {{XL6_CAPTURE_TRIVIAL,offsetof(Payload,loan),sizeof(void*)},SIZE_MAX},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Payload,number),sizeof(uint64_t)},SIZE_MAX}};
static const size_t order[]={4,0,7,6,3,1,5,2};
static const XL6_ActivationCaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),10,slots,8,order};
static const XL6_ActivationCaptureRuntimeV1* api;
static unsigned freezes,drops,sequence[3],sequenceCount,edges,sparseCases,orderedCases;
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
    xrtownershipscope freeze={0};(void)ignored;
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&xrtOwnershipScopeEnd(&freeze));return 0;
}
typedef struct DropData { unsigned id,mode; unsigned char* flag; XL6_Generator** slot; } DropData;
static void drop(void* data)
{
    DropData d=*(DropData*)data;
    assert(!d.flag||(!*d.flag&&!*d.slot));
    HANDLE thread=CreateThread(NULL,0,freeze_probe,NULL,0,NULL);
    assert(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0&&CloseHandle(thread));++freezes;++drops;
    if(d.id<3){assert(sequenceCount<3);sequence[sequenceCount++]=d.id;}
    xl6GeneratorRuntimeV1Get()->FrameFree(data);
    if(d.mode==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else if(d.mode==2)xrtSetErrorKind(XERR_INTERNAL);
}
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static bool resume(XL6_Generator* g){xl6GeneratorRuntimeV1Get()->Finish(g);return false;}
static void invoke(void){}
static XL6_Generator* make_generator(unsigned id,unsigned mode,unsigned char* flag,XL6_Generator** slot)
{
    DropData* d=xl6GeneratorRuntimeV1Get()->FrameAllocate(sizeof(*d));assert(d);*d=(DropData){id,mode,flag,slot};
    XL6_Generator* g=xl6GeneratorRuntimeV1Get()->Create(NULL,d,resume,drop,sizeof(int64_t),NULL,empty_trace,empty_trace);assert(g);return g;
}
static bool edge(xrtownershipref ref,void* ignored)
{
    size_t count;(void)ignored;assert(ref.Data&&ref.Ops&&ref.Ops->Count(ref.Data,&count)&&count);++edges;return true;
}
static bool refuse_edge(xrtownershipref ref,void* ignored){(void)ref;(void)ignored;return false;}
static void sparse(unsigned mask)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Payload p={0};
    p.loan=(void*)(uintptr_t)1;p.number=UINT64_C(0xfedcba9876543210);
    /* Inactive residents deliberately retain poison: flags must be checked
     * before loading or tracing the pointer; protected loans never own it. */
    p.string=(char*)(uintptr_t)1;p.value=(xvalue*)(uintptr_t)1;p.future=(xfuture*)(uintptr_t)1;
    p.closure=(XL6_Closure*)(uintptr_t)1;p.error=(xerror*)(uintptr_t)1;p.cell=(XL6_Cell*)(uintptr_t)1;
    p.iterator=(XL6_ValueIterator*)(uintptr_t)1;p.generator=(XL6_Generator*)(uintptr_t)1;
    for(unsigned i=0;i<8;++i)p.initialized[i]=(mask>>i)&1;
    if(mask&1){p.string=xrtStrDup("sparse");assert(p.string);}
    if(mask&2){p.value=xrtValueInt(17);assert(p.value);}
    if(mask&4){xpromise* promise=xrtPromiseCreate(&p.future,NULL);assert(promise&&p.future);xrtPromiseDestroy(promise);}
    if(mask&8){p.closure=xl6ClosureRuntimeV1Get()->Create(NULL,invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);assert(p.closure);}
    if(mask&16){p.error=xrtErrorCreate(XERR_STATE,"sparse",7,"sparse");assert(p.error);}
    if(mask&32){p.cell=xl6CellRuntimeV1Get()->Create(XL6_CELL_I64,sizeof(int64_t));int64_t value=19;assert(p.cell);xl6CellRuntimeV1Get()->Initialize(p.cell,&value,sizeof(value));}
    if(mask&64){xvalue* source=xrtValueArray();assert(source&&xrtValueArrayAppendNew(source,xrtValueInt(23)));
        p.iterator=xl6ValueIteratorRuntimeV1Get()->Create(source);assert(p.iterator);xrtValueRelease(source);}
    if(mask&128)p.generator=make_generator(3,0,&p.initialized[7],&p.generator);
    unsigned expected=0;for(unsigned i=1;i<8;++i)expected+=(mask>>i)&1;
    edges=0;assert(api->Trace(&layout,&p,edge,NULL)&&edges==expected);
    if(expected)assert(!api->Trace(&layout,&p,refuse_edge,NULL));
    Payload original=p;
    assert(xrtMemDebugFailAfter(0));
    xl6_error_result result=api->Cleanup(&layout,&p,(xl6_error_result){true,NULL});
    assert(result.ok&&!result.error&&!xrtMemDebugFailTriggered());
    result=api->Cleanup(&layout,&p,result);assert(result.ok&&!result.error&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    for(unsigned i=0;i<8;++i){assert(!p.initialized[i]);void* actual=NULL;memcpy(&actual,(char*)&p+slots[i].value.offset,sizeof(actual));
        assert(actual==((mask&(1u<<i))?NULL:(void*)(uintptr_t)1));}
    assert(p.loan==original.loan&&p.number==original.number);edges=0;assert(api->Trace(&layout,&p,edge,NULL)&&!edges);
    ++sparseCases;balance(&before);
}
typedef struct Ordered {XL6_Generator* child[3];unsigned char live[3];} Ordered;
static void ordered(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Ordered p={0};sequenceCount=0;
    XL6_ActivationCaptureSlot s[3];const size_t cleanup[]={1,2,0};
    XL6_ActivationCaptureLayoutV1 l={sizeof(l),sizeof(p),3,s,3,cleanup};
    for(unsigned i=0;i<3;++i){s[i]=(XL6_ActivationCaptureSlot){{XL6_CAPTURE_GENERATOR,offsetof(Ordered,child)+i*sizeof(p.child[0]),sizeof(p.child[0])},offsetof(Ordered,live)+i};
        p.live[i]=1;p.child[i]=make_generator(i,i==1?1:2,&p.live[i],&p.child[i]);}
    xerror* ambient=xrtErrorCreate(XERR_ARGUMENT,"ambient",11,"ambient");assert(ambient);xrtSetErrorTake(ambient);
    xerror* original=mode==2?xrtErrorCreate(XERR_STATE,"primary",12,"primary"):NULL;assert(mode!=2||original);
    xl6_error_result result=api->Cleanup(&l,&p,(xl6_error_result){mode==0,original});
    assert(!result.ok&&result.error==original&&xrtGetError()==ambient);
    assert(sequenceCount==3&&sequence[0]==1&&sequence[1]==2&&sequence[2]==0);
    result=api->Cleanup(&l,&p,result);assert(!result.ok&&result.error==original&&xrtGetError()==ambient&&sequenceCount==3);
    xrtErrorFree(result.error);xrtClearError();++orderedCases;balance(&before);
}
static void invalid_layouts(void)
{
    XL6_ActivationCaptureSlot s[10];size_t cleanup[8];
    memcpy(s,slots,sizeof(s));memcpy(cleanup,order,sizeof(cleanup));
    XL6_ActivationCaptureLayoutV1 l=layout;l.slots=s;l.cleanup=cleanup;
    assert(api->Validate(&l,sizeof(Payload)));
#define BAD(EXPRESSION,RESTORE) do{EXPRESSION;assert(!api->Validate(&l,sizeof(Payload)));RESTORE;}while(0)
    BAD(l.size=0,l.size=sizeof(l));BAD(l.bytes=0,l.bytes=sizeof(Payload));
    BAD(l.count=SIZE_MAX,l.count=10);BAD(l.slots=NULL,l.slots=s);
    BAD(l.cleanup=NULL,l.cleanup=cleanup);BAD(l.cleanupCount=7,l.cleanupCount=8);
    BAD(cleanup[0]=10,cleanup[0]=order[0]);BAD(cleanup[0]=cleanup[1],cleanup[0]=order[0]);
    BAD(cleanup[0]=8,cleanup[0]=order[0]);
    for(unsigned i=0;i<10;++i){XL6_ActivationCaptureSlot saved=s[i];
        BAD(s[i].value.kind=(XL6_CaptureKind)99,s[i]=saved);
        BAD(s[i].value.offset=SIZE_MAX,s[i]=saved);BAD(s[i].value.bytes=SIZE_MAX,s[i]=saved);
        BAD(s[i].value.bytes=0,s[i]=saved);
        if(i<8){BAD(s[i].initialized=SIZE_MAX,s[i]=saved);BAD(s[i].initialized=sizeof(Payload),s[i]=saved);
            BAD(s[i].initialized=s[(i+1)%8].initialized,s[i]=saved);BAD(s[i].initialized=slots[9].value.offset,s[i]=saved);}
        else BAD(s[i].initialized=0,s[i]=saved);
    }
#undef BAD
    Payload p={0};p.initialized[7]=2;edges=0;assert(!api->Trace(&layout,&p,edge,NULL)&&!edges);
    XL6_ActivationCaptureLayoutV1 empty={sizeof(empty),1,0,NULL,0,NULL};unsigned char byte=99;
    assert(api->Validate(&empty,1)&&api->Trace(&empty,&byte,edge,NULL));
    assert(api->Cleanup(&empty,&byte,(xl6_error_result){true,NULL}).ok&&byte==99);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6ActivationCaptureRuntimeV1Get();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    invalid_layouts();for(unsigned mask=0;mask<256;++mask)sparse(mask);
    for(unsigned n=0;n<100;++n)for(unsigned mode=0;mode<3;++mode)ordered(mode);
    assert(sparseCases==256&&orderedCases==300&&drops==1028&&freezes==1028);
    puts("Activation capture layouts: 256 sparse owning masks, 300 exact-order first-failure transactions, 1028 detached callback freezes; poisoned loans untouched, repeated cleanup and armed OOM allocation-free, exact memory balance");return 0;
}
