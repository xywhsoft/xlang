#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_slot.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void slot_test_write(int64_t);
extern int64_t slot_test_read(void);

typedef struct Slots { int32 refs; XL6_Cell* a; bool ia; XL6_Cell* b; bool ib; } Slots;
typedef struct Work { Slots* slots; volatile LONG ready,done; int mode; int64_t value; } Work;
static const XL6_SlotRuntimeV1* slotApi;
static const XL6_CellRuntimeV1* cellApi;
static LONG flag(volatile LONG* p) { return InterlockedCompareExchange(p,0,0); }
static void wait_flag(volatile LONG* p) {
    DWORD start=GetTickCount(); while(!flag(p)) { assert((DWORD)(GetTickCount()-start)<10000); SwitchToThread(); }
}
static void joined(HANDLE thread) {
    DWORD code; assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && GetExitCodeThread(thread,&code) && !code); CloseHandle(thread);
}
static bool empty(const xrtownershipscope* scope)
{ return !scope->Self && !scope->Parent && !scope->Thread && !scope->Children && !scope->Mode; }
static bool slots_count(const void* data,size_t* count) {
    const Slots* s=data; if(!s || !count || s->refs<=0)return false; *count=(size_t)s->refs; return true;
}
static bool slots_trace(const void* data,xrtownershipvisitor visit,void* context) {
    const Slots* s=data;
    if(!s || !visit)return false;
    return (!s->ia || visit(cellApi->Ownership(s->a),context)) && (!s->ib || visit(cellApi->Ownership(s->b),context));
}
static const xrtownershipops slots_ops={slots_count,slots_trace};
static void graph(const Slots* s) {
    xrtownershipref root={s,&slots_ops}; xrtownershipresult result={0}; bool reachable=false;
    assert(s->ia!=s->ib && (s->ia?(s->a && !s->b):(!s->a && s->b)));
    assert(xrtOwnershipInspectReachable(&root,1,NULL,0,&reachable,&result,NULL,NULL) && reachable &&
        result.NodeCount==4 && result.EdgeCount==3 && result.ExternalRootCount==1);
    assert(xrtOwnershipInspectReachable(&root,1,&root,1,&reachable,&result,NULL,NULL) && !reachable &&
        result.NodeCount==4 && result.EdgeCount==4 && !result.ExternalRootCount);
}
static void handoff(Slots* s,bool reverse) {
    XL6_Cell** from=reverse?&s->b:&s->a; XL6_Cell** to=reverse?&s->a:&s->b;
    bool* sourceBit=reverse?&s->ib:&s->ia; bool* targetBit=reverse?&s->ia:&s->ib;
    const XL6_Cell* nothing=NULL; const bool inactive=false;
    const XL6_SlotWrite writes[]={
        {to,from,sizeof(*to)},{targetBit,sourceBit,sizeof(*targetBit)},
        {from,&nothing,sizeof(*from)},{sourceBit,&inactive,sizeof(*sourceBit)}};
    slotApi->Apply(writes,4);
}
static DWORD WINAPI run(void* data) {
    Work* w=data;
    if(w->mode!=2) {
        xrtownershipscope refused={0}; assert(!xrtOwnershipFreezeTryBegin(&refused) && empty(&refused));
    }
    InterlockedExchange(&w->ready,1);
    if(w->mode==0) slot_test_write(w->value);
    else if(w->mode==1) handoff(w->slots,false);
    else for(unsigned i=0;i<10000;++i) { handoff(w->slots,false); handoff(w->slots,true); SwitchToThread(); }
    InterlockedExchange(&w->done,1); return 0;
}
static void balanced(const xmemdebugsnapshot* before) {
    xmemdebugsnapshot after; assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
}
static void generated(unsigned count) {
    slot_test_write(0); assert(slot_test_read()==0);
    for(unsigned i=0;i<count;++i) {
        Work w={0}; xrtownershipscope freeze={0}; w.value=i+1;
        assert(xrtOwnershipFreezeTryBegin(&freeze));
        HANDLE thread=CreateThread(NULL,0,run,&w,0,NULL); assert(thread); wait_flag(&w.ready);
        for(unsigned wait=0;wait<4000;++wait)SwitchToThread();
        if(flag(&w.done) || slot_test_read()!=(int64_t)i) fputs("generated shared slot escaped freeze\n",stderr);
        assert(!flag(&w.done) && slot_test_read()==(int64_t)i);
        assert(xrtOwnershipScopeEnd(&freeze)); joined(thread); assert(slot_test_read()==w.value);
    }
}
int main(int argc,char** argv) {
    assert(xrtMemDebugEnable(true)); slotApi=xl6SlotRuntimeV1Get(); cellApi=xl6CellRuntimeV1Get();
    if(argc==2 && !strcmp(argv[1],"--generated-only")) { generated(1); return 0; }
    assert(argc==1); xmemdebugsnapshot initial; xrtMemDebugSnapshot(&initial);
    generated(100);
    Slots s={0}; s.refs=1;
    xvalue* value=xrtValueArray(); assert(value && xrtValueReserve(value,4));
    s.a=cellApi->Create(XL6_CELL_XVALUE,sizeof(value)); assert(s.a); cellApi->Initialize(s.a,&value,sizeof(value)); s.ia=true;
    for(unsigned i=0;i<100;++i) {
        Work w={0}; xrtownershipscope freeze={0}; w.mode=1; w.slots=&s;
        assert(xrtOwnershipFreezeTryBegin(&freeze)); graph(&s);
        HANDLE thread=CreateThread(NULL,0,run,&w,0,NULL); assert(thread); wait_flag(&w.ready);
        for(unsigned wait=0;wait<2000;++wait)SwitchToThread();
        assert(!flag(&w.done) && s.ia && !s.ib); graph(&s);
        assert(xrtOwnershipScopeEnd(&freeze)); joined(thread); graph(&s);
        assert(!s.ia && s.ib);
        /* Applying a transfer must not allocate or consume the armed failure. */
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(0));
        handoff(&s,true); assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtMemDebugSnapshot(&after);
        assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount); graph(&s);
    }
    Work racing={0}; racing.mode=2; racing.slots=&s;
    HANDLE writer=CreateThread(NULL,0,run,&racing,0,NULL); assert(writer); wait_flag(&racing.ready);
    unsigned probes=0;
    while(!flag(&racing.done)) {
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)) { graph(&s); ++probes; assert(xrtOwnershipScopeEnd(&freeze)); }
        else assert(empty(&freeze));
        SwitchToThread();
    }
    joined(writer); assert(probes>0 && s.ia && !s.ib); graph(&s);
    /* Ordered reads must observe preceding writes, not an implicit parallel
     * copy; same-address assignment is valid. Empty values use typed objects. */
    int a=7,b=0,c=0; const XL6_SlotWrite chain[]={{&b,&a,sizeof(a)},{&c,&b,sizeof(b)},{&c,&c,sizeof(c)}};
    slotApi->Apply(chain,3); slotApi->Apply(NULL,0); assert(a==7 && b==7 && c==7);
    XL6_Cell* taken=NULL; const XL6_SlotWrite take[]={{&taken,&s.a,sizeof(taken)},
        {&s.a,&(XL6_Cell*){NULL},sizeof(s.a)},{&s.ia,&(bool){false},sizeof(s.ia)}};
    slotApi->Apply(take,3); assert(taken && !s.a && !s.ia); cellApi->Release(taken); s.refs=0;
    balanced(&initial);
    printf("Slot transfers: 100 generated global freezes, 100 frozen physical handoffs, 100 armed-OOM allocation-free commits, 20000 racing handoffs, %u frozen graph probes; exact active/payload/ref/allocator balance, no native root registration\n",probes);
    return 0;
}
